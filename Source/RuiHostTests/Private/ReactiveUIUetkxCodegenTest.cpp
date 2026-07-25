// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Codegen — compiles real .uetkx sources through the full pipeline
// (file scan → markup parse → C++ emit) and pins the generated shapes: props struct +
// Equals, hook auto-prefixing (Ctx.*), the baked __RUI_HOOK_SIG, NSLOCTEXT for text,
// registration, control-flow lowering, and the diagnostics that must fail a compile.
// (The generated code COMPILING is proven end-to-end by the gallery conversion milestone —
// its .inl files build into RuiDemo through the aggregator.)

#include "Misc/AutomationTest.h"
#include "UetkxCodegen.h"
#include "UetkxFileScan.h"
#include "UetkxResolve.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** FILE_SCOPED_EXPORTS: import qualification needs the TARGET file's surface (its namespace +
	 *  decl kinds), so the alias/shadow pins compile against this in-memory resolver instead of
	 *  the old resolver-less plane. Specifier "./X" resolves to key "X", label "X.uetkx" —
	 *  namespaces derive as `RuiUetkx_X`, the same convention the contract harness pins. */
	class FUetkxCodegenTestResolver final : public IUetkxImportResolver
	{
	public:
		TMap<FString, TMap<FString, FUetkxTargetDecl>> Files; // key -> exported surface
		TMap<FString, FString> Defaults;					  // key -> default-export name

		virtual FString Resolve(const FString& Spec, const FString&) const override
		{
			FString Key = Spec;
			Key.RemoveFromStart(TEXT("./"));
			return Files.Contains(Key) ? Key : FString();
		}
		virtual bool GetDecls(const FString& Key, TMap<FString, FUetkxTargetDecl>& Out) const override
		{
			if (const TMap<FString, FUetkxTargetDecl>* F = Files.Find(Key))
			{
				Out = *F;
				return true;
			}
			return false;
		}
		virtual FString DefaultExportOf(const FString& Key) const override { return Defaults.FindRef(Key); }
		virtual bool CrossesModuleBoundary(const FString&, const FString&) const override { return false; }
		virtual uint32 ExportHashOf(const FString&) const override { return 1u; }
		virtual FString LabelForKey(const FString& Key) const override { return Key + TEXT(".uetkx"); }
		virtual FString WouldBeLabel(const FString& Spec, const FString&) const override
		{
			FString Key = Spec;
			Key.RemoveFromStart(TEXT("./"));
			return Key + TEXT(".uetkx");
		}
		virtual FString FindExporter(const FString& Name, const FString&, EUetkxDeclKind& OutKind) const override
		{
			for (const TPair<FString, TMap<FString, FUetkxTargetDecl>>& File : Files)
			{
				if (const FUetkxTargetDecl* D = File.Value.Find(Name); D != nullptr && D->bExported)
				{
					OutKind = D->Kind;
					return File.Key;
				}
			}
			return FString();
		}
		virtual FString SuggestSpecifier(const FString&, const FString& Key) const override { return TEXT("./") + Key; }
	};

	/** The shared alias-plane fixture surface: Palette2 (values Cool/Accent + default component
	 *  PalCard), StatusChip (component), Hooks2 (hook UseCounter). */
	FUetkxCodegenTestResolver MakeAliasResolver()
	{
		FUetkxCodegenTestResolver R;
		auto Decl = [](EUetkxDeclKind K)
		{
			FUetkxTargetDecl D;
			D.Kind = K;
			D.bExported = true;
			return D;
		};
		TMap<FString, FUetkxTargetDecl>& Pal = R.Files.Add(TEXT("Palette2"));
		Pal.Add(TEXT("Cool"), Decl(EUetkxDeclKind::Value));
		Pal.Add(TEXT("Accent"), Decl(EUetkxDeclKind::Value));
		Pal.Add(TEXT("PalCard"), Decl(EUetkxDeclKind::Component));
		R.Defaults.Add(TEXT("Palette2"), TEXT("PalCard"));
		R.Files.Add(TEXT("StatusChip")).Add(TEXT("StatusChip"), Decl(EUetkxDeclKind::Component));
		R.Files.Add(TEXT("Hooks2")).Add(TEXT("UseCounter"), Decl(EUetkxDeclKind::Hook));
		return R;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxCodegenTest, "ReactiveUI.Uetkx.Codegen",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxCodegenTest::RunTest(const FString&)
{
	// ── the counter (the family's hello-world of state) ───────────────────────────────────
	{
		const FString Source = TEXT(R"UETKX(
export component Counter(StartAt: int32 = 0) {
	auto [Count, SetCount] = UseState(StartAt);
	TFunction<void(int32)> Set = SetCount;
	const int32 Now = Count;
	return (
		<VerticalBox>
			<TextBlock Text={ FText::AsNumber(Count) } />
			<Button OnClicked={ Set(Now + 1) }>+1</Button>
			@if (Count > 3) {
				return ( <TextBlock Text="big!" /> )
			}
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("Counter"));
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			AddInfo(FString::Printf(TEXT("diag %s: %s @%d"), *Diag.Code, *Diag.Message, Diag.Offset));
		}
		if (!TestTrue(TEXT("counter compiles"), Out.bOk))
		{
			return false;
		}
		TestTrue(TEXT("props struct emitted"),
				 Out.Inl.Contains(TEXT("struct FCounterUetkxProps final : public FRuiPropsBase")));
		TestTrue(TEXT("param with default"), Out.Inl.Contains(TEXT("int32 StartAt = 0;")));
		TestTrue(TEXT("hook auto-prefixed"), Out.Inl.Contains(TEXT("Ctx.UseState(StartAt)")));
		TestTrue(TEXT("hashed BODY holds the markup (TB-23 — unique lambda manglings per generation)"),
				 Out.Inl.Contains(TEXT("static FRuiNodeArray Counter_UetkxBody_")));
		TestTrue(TEXT("STABLE impl shim (TB-23 — the registered/redirect anchor must never rename)"),
				 Out.Inl.Contains(TEXT("static FRuiNodeArray Counter_UetkxImpl(FRuiContext& Ctx, "
									   "const FCounterUetkxProps& Props")));
		TestTrue(
			TEXT("registration emitted (FQN runtime identity via the STABLE shim, FS-04)"),
			Out.Inl.Contains(TEXT(
				"RUI::RegisterComponentId((void*)&Counter_UetkxImpl, FName(TEXT(\"RuiUetkx_Counter::Counter\")))")));
		// TB-23 invariant: an EDIT re-hashes the body but the registered pointer symbol stays
		// stable — Live Coding redirection (HMR's engine) rides the stable name; the first-cut
		// hashed impl froze HMR (old fibers invoked dead code forever). Pinned both ways.
		{
			const FUetkxCompileOutput Edited = FUetkxCodegen::CompileSource(Source + TEXT("\n"), TEXT("Counter"));
			if (TestTrue(TEXT("edited counter compiles"), Edited.bOk))
			{
				TestTrue(TEXT("edited generation still registers the STABLE impl symbol"),
						 Edited.Inl.Contains(TEXT("RegisterComponentId((void*)&Counter_UetkxImpl,")));
				auto BodyNameOf = [](const FString& Inl) -> FString
				{
					const int32 At = Inl.Find(TEXT("Counter_UetkxBody_"));
					return At >= 0 ? Inl.Mid(At, 26) : FString();
				};
				TestNotEqual(TEXT("edited generation gets a DIFFERENT body name (lambda manglings never collide)"),
							 BodyNameOf(Out.Inl), BodyNameOf(Edited.Inl));
			}
		}
		TestTrue(TEXT("hook sig baked"), Out.Inl.Contains(TEXT("Counter_RUI_HOOK_SIG = 0x")));
		TestTrue(TEXT("wrapper for cross-component refs"), Out.Inl.Contains(TEXT("inline FRuiNode Counter(")));
		TestTrue(TEXT("event lowered with the Value payload"),
				 Out.Inl.Contains(
					 TEXT("P.SetOnClicked(FRuiCallback::Create([=](const FRuiValue& Value) { Set(Now + 1); }))")));
		TestTrue(TEXT("text child NSLOCTEXT"), Out.Inl.Contains(TEXT("NSLOCTEXT(\"Uetkx.Counter\"")));
		TestTrue(TEXT("@if lowered to if"), Out.Inl.Contains(TEXT("if (Count > 3)")));
		TestTrue(TEXT("factory targeted"), Out.Inl.Contains(TEXT("RUI::Slate::VerticalBox(MoveTemp(P), MoveTemp(Ch)")));
		TestTrue(TEXT("named factory self-registers under the FQN"),
				 Out.Inl.Contains(TEXT("RUI::RegisterNamedFactory(FName(TEXT(\"RuiUetkx_Counter::Counter\")), []() { "
									   "return Counter(); })")));
		TestEqual(TEXT("hook sig from one UseState"), Out.HookSig, FUetkxFileScan::HookSignature({TEXT("UseState")}));
	}

	// ── `return null;` — render-nothing (TB-28: React/Unity family parity) ────────────────
	{
		const FString Source = TEXT(R"UETKX(
export FRuiNode Gate(bool bHidden = false) {
	if (bHidden) {
		return null;
	}
	return ( <Spacer /> );
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("Gate"));
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			AddInfo(FString::Printf(TEXT("diag %s: %s @%d"), *Diag.Code, *Diag.Message, Diag.Offset));
		}
		if (TestTrue(TEXT("early `return null;` compiles"), Out.bOk))
		{
			TestTrue(TEXT("null span lowers to an EMPTY node array (renders nothing)"),
					 Out.Inl.Contains(TEXT("return {};")));
			TestTrue(TEXT("the verbatim if-guard splices around it"), Out.Inl.Contains(TEXT("if (bHidden)")));
		}
		const FUetkxCompileOutput Empty =
			FUetkxCodegen::CompileSource(TEXT("export FRuiNode Empty() {\n\treturn null;\n}\n"), TEXT("Empty"));
		if (TestTrue(TEXT("null-only component compiles (single-return emitter path)"), Empty.bOk))
		{
			TestTrue(TEXT("null-only lowers to an empty array return"), Empty.Inl.Contains(TEXT("return {};")));
		}
		const FUetkxCompileOutput Paren =
			FUetkxCodegen::CompileSource(TEXT("export FRuiNode Empty2() {\n\treturn ( null );\n}\n"), TEXT("Empty2"));
		TestTrue(TEXT("paren form `return ( null );` compiles"), Paren.bOk);
	}

	// ── keyed @for + style keys + Slot.* + cross-component reference ──────────────────────
	{
		const FString Source = TEXT(R"UETKX(
component RowList(Names: TArray<FString>) {
	return (
		<VerticalBox>
			@for (int32 i = 0; i < Names.Num(); ++i) {
				return ( <TextBlock key={i} Text={ FText::FromString(Names[i]) } RenderOpacity={0.5f} Slot.Padding="0,4,0,0" /> )
			}
			<StatusPanel Label="footer" />
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("RowList"));
		if (!TestTrue(TEXT("rowlist compiles"), Out.bOk))
		{
			return false;
		}
		TestTrue(TEXT("@for lowered"), Out.Inl.Contains(TEXT("for (int32 i = 0; i < Names.Num(); ++i)")));
		TestTrue(TEXT("key expr lowered"), Out.Inl.Contains(TEXT("FRuiKey(i)")));
		TestTrue(TEXT("style key on TextBlock"),
				 Out.Inl.Contains(TEXT("__Style->Add(FName(TEXT(\"RenderOpacity\")), FRuiValue(0.5f))")));
		TestTrue(TEXT("slot key routed to SlotProps"),
				 Out.Inl.Contains(TEXT("__Slot->Add(FName(TEXT(\"Slot.Padding\")), FRuiValue(TEXT(\"0,4,0,0\")))")));
		TestTrue(TEXT("cross-component call emitted"), Out.Inl.Contains(TEXT("FStatusPanelUetkxProps P;")));
		TestTrue(TEXT("component prop assigned"), Out.Inl.Contains(TEXT("P.Label = TEXT(\"footer\");")));
	}

	// ── leaf widgets: (Props, Key) arity, no children; classes is a universal attr ────────
	{
		FUetkxCompileOutput Leaf = FUetkxCodegen::CompileSource(
			TEXT(
				"component Leafy { return ( <VerticalBox classes=\"card dim\"> <Spacer Size={ FVector2D(1.f, 6.f) } /> "
				"</VerticalBox> ); }"),
			TEXT("Leafy"));
		if (TestTrue(TEXT("leafy compiles"), Leaf.bOk))
		{
			TestTrue(TEXT("leaf factory takes no children"),
					 Leaf.Inl.Contains(TEXT("return RUI::Slate::Spacer(MoveTemp(P), FRuiKey());")));
			TestTrue(TEXT("classes lowered"), Leaf.Inl.Contains(TEXT("P.Classes.Add(FName(TEXT(\"card\")));")) &&
												  Leaf.Inl.Contains(TEXT("P.Classes.Add(FName(TEXT(\"dim\")));")));
		}
		FUetkxCompileOutput Bad = FUetkxCodegen::CompileSource(
			TEXT("component Nope { return ( <Spacer><TextBlock Text=\"x\" /></Spacer> ); }"), TEXT("Nope"));
		TestTrue(TEXT("children on a leaf widget fail (3005)"),
				 !Bad.bOk &&
					 Bad.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX3005"); }));
	}

	// ── diagnostics that fail the compile ─────────────────────────────────────────────────
	{
		{
			FUetkxCompileOutput Lower =
				FUetkxCodegen::CompileSource(TEXT("component tiny { return ( <Spacer /> ); }"), TEXT("tiny"));
			TestTrue(TEXT("2100 lowercase component"),
					 Lower.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX2100"); }));
		}
		FUetkxCompileOutput NoReturn =
			FUetkxCodegen::CompileSource(TEXT("component Empty { int32 A = 1; }"), TEXT("Empty"));
		TestTrue(TEXT("2101 no markup return"), !NoReturn.bOk && NoReturn.Diags.Last().Code == TEXT("UETKX2101"));
		FUetkxCompileOutput BadAttr =
			FUetkxCodegen::CompileSource(TEXT("component Bad { return ( <Button Bogus=\"x\" /> ); }"), TEXT("Bad"));
		TestTrue(TEXT("0105 unknown attribute"), !BadAttr.bOk);
		bool bFound0105 = false;
		for (const FUetkxDiag& Diag : BadAttr.Diags)
		{
			bFound0105 |= Diag.Code == TEXT("UETKX0105");
		}
		TestTrue(TEXT("0105 code present"), bFound0105);

		// R10 — UETKX0106 invalid enum value: the runtime parses these with SILENT fallbacks
		// (ParseHAlign et al), so the compiler is the only build-time backstop. Vocabularies
		// include fallback-only spellings ("fill") and match case-insensitively (FName).
		FUetkxCompileOutput BadEnum = FUetkxCodegen::CompileSource(
			TEXT("component Bad2 { return ( <Border HAlign=\"cesssssnter\"><Spacer /></Border> ); }"), TEXT("Bad2"));
		TestTrue(TEXT("0106 invalid enum value on element attr"),
				 !BadEnum.bOk && BadEnum.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																   { return D.Code == TEXT("UETKX0106"); }));
		FUetkxCompileOutput BadSlotEnum = FUetkxCodegen::CompileSource(
			TEXT("component Bad3 { return ( <VerticalBox><Spacer Slot.VAlign=\"botom\" /></VerticalBox> ); }"),
			TEXT("Bad3"));
		TestTrue(TEXT("0106 invalid enum value on slot key"),
				 !BadSlotEnum.bOk && BadSlotEnum.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																		   { return D.Code == TEXT("UETKX0106"); }));
		FUetkxCompileOutput GoodEnum = FUetkxCodegen::CompileSource(
			TEXT("component Ok6 { return ( <Border HAlign=\"Fill\" Slot.HAlign=\"center\" Padding=\"4\">")
				TEXT("<TextBlock Text=\"t\" Justification=\"center\" /></Border> ); }"),
			TEXT("Ok6"));
		TestTrue(TEXT("valid enum values (incl. fallback-only 'Fill', case-insensitive) compile"), GoodEnum.bOk);

		// R11 — typed style/slot strings: malformed literals used to Atof to 0/false silently.
		FUetkxCompileOutput BadFormat = FUetkxCodegen::CompileSource(
			TEXT("component Bad4 { return ( <Spacer RenderOpacity=\"abc\" /> ); }"), TEXT("Bad4"));
		TestTrue(TEXT("0106 malformed float style string"),
				 !BadFormat.bOk && BadFormat.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																	   { return D.Code == TEXT("UETKX0106"); }));
		FUetkxCompileOutput BadColor = FUetkxCodegen::CompileSource(
			TEXT("component Bad5 { return ( <TextBlock Text=\"t\" ColorAndOpacity=\"red\" /> ); }"), TEXT("Bad5"));
		TestTrue(TEXT("0106 color has no string form (TextBlock fast path)"),
				 !BadColor.bOk && BadColor.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																	 { return D.Code == TEXT("UETKX0106"); }));
		// the TextBlock fast path duplicates the style lowering — pin its enum check too
		FUetkxCompileOutput BadTbEnum = FUetkxCodegen::CompileSource(
			TEXT("component Bad7 { return ( <TextBlock Text=\"t\" Justification=\"centre\" /> ); }"), TEXT("Bad7"));
		TestTrue(TEXT("0106 invalid enum value on the TextBlock fast path"),
				 !BadTbEnum.bOk && BadTbEnum.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																	   { return D.Code == TEXT("UETKX0106"); }));
		FUetkxCompileOutput BadFlag =
			FUetkxCodegen::CompileSource(TEXT("component Bad6 { return ( <Spacer RenderOpacity /> ); }"), TEXT("Bad6"));
		TestTrue(TEXT("0106 flag form on a float style key"),
				 !BadFlag.bOk && BadFlag.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																   { return D.Code == TEXT("UETKX0106"); }));
		// R12 — duplicate attrs (last-wins was silent), duplicate literal sibling keys (silent
		// remount + state loss), and slot keys the parent's slot-apply never reads.
		FUetkxCompileOutput DupAttr = FUetkxCodegen::CompileSource(
			TEXT("component Bad8 { return ( <Border Padding=\"1\" Padding=\"2\"><Spacer /></Border> ); }"),
			TEXT("Bad8"));
		TestTrue(TEXT("0109 duplicate attribute"),
				 !DupAttr.bOk && DupAttr.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																   { return D.Code == TEXT("UETKX0109"); }));
		FUetkxCompileOutput DupKey = FUetkxCodegen::CompileSource(
			TEXT("component Bad9 { return ( <VerticalBox><Spacer key=\"a\" /><Spacer key=\"a\" />")
				TEXT("</VerticalBox> ); }"),
			TEXT("Bad9"));
		TestTrue(TEXT("0110 duplicate sibling key"),
				 !DupKey.bOk &&
					 DupKey.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX0110"); }));
		FUetkxCompileOutput BadSlot = FUetkxCodegen::CompileSource(
			TEXT("component Bad10 { return ( <VerticalBox><Spacer Slot.ZOrder=\"2\" /></VerticalBox> ); }"),
			TEXT("Bad10"));
		TestTrue(TEXT("0111 slot key the parent never reads"),
				 !BadSlot.bOk && BadSlot.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																   { return D.Code == TEXT("UETKX0111"); }));
		FUetkxCompileOutput GoodSlots = FUetkxCodegen::CompileSource(
			TEXT("component Ok8 { return ( <GridPanel><Spacer Slot.Column=\"1\" Slot.Row=\"0\" /></GridPanel> ); }"),
			TEXT("Ok8"));
		TestTrue(TEXT("Slot.Column/Slot.Row are REAL GridPanel keys (schema-canon fix) and compile"), GoodSlots.bOk);

		// R13 — brush names: closed per engine (FCoreStyle chain); the environment set is
		// injected. A deterministic vocabulary here; disarmed (empty) again after — the
		// editor module re-injects the real set per process, and markup-compiling
		// commandlets run in their own processes.
		FUetkxCodegen::SetEnvironmentBrushNames({TEXT("WhiteBrush"), TEXT("GenericWhiteBox")});
		FUetkxCompileOutput BadBrush = FUetkxCodegen::CompileSource(
			TEXT("component Bad11 { return ( <Border BorderImage=\"WhissssssteBrush\"><Spacer /></Border> ); }"),
			TEXT("Bad11"));
		TestTrue(TEXT("0106 unknown brush name"),
				 !BadBrush.bOk && BadBrush.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																	 { return D.Code == TEXT("UETKX0106"); }));
		FUetkxCompileOutput GoodBrush = FUetkxCodegen::CompileSource(
			TEXT("component Ok9 { return ( <Border BorderImage=\"whitebrush\"><Spacer /></Border> ); }"), TEXT("Ok9"));
		TestTrue(TEXT("registered brush compiles (case-insensitive, FName semantics)"), GoodBrush.bOk);
		FUetkxCodegen::SetEnvironmentBrushNames({});
		FUetkxCompileOutput NoEnv = FUetkxCodegen::CompileSource(
			TEXT("component Ok10 { return ( <Border BorderImage=\"AnythingGoes\"><Spacer /></Border> ); }"),
			TEXT("Ok10"));
		TestTrue(TEXT("un-injected environment disarms the brush check"), NoEnv.bOk);

		// R14 — canonical casing (UETKX0112): `slot.fill` used to route into the slot dict via
		// the IgnoreCase prefix (silently working while every exact-case check disarmed);
		// wrong-cased style keys were unknown-attr noise; wrong-cased ELEMENT attrs silently
		// matched through the FName lookup. All three now correct by name.
		FUetkxCompileOutput Miscased = FUetkxCodegen::CompileSource(
			TEXT("component Bad12 { return ( <VerticalBox><Spacer slot.fill=\"1\" /><Box halign=\"center\">")
				TEXT("<Spacer renderopacity=\"0.5\" /></Box></VerticalBox> ); }"),
			TEXT("Bad12"));
		int32 NumCasing = 0;
		for (const FUetkxDiag& Diag : Miscased.Diags)
		{
			NumCasing += Diag.Code == TEXT("UETKX0112") ? 1 : 0;
		}
		TestTrue(TEXT("0112 on wrong-cased slot key, element attr, and style key"), !Miscased.bOk && NumCasing == 3);

		FUetkxCompileOutput GoodForms = FUetkxCodegen::CompileSource(
			TEXT("component Ok7 { return ( <VerticalBox RenderOpacity=\"0.5\" Enabled RenderTranslation=\"5,7\">")
				TEXT("<Spacer Slot.Padding=\"1,2\" Slot.Fill=\"1\" />")
					TEXT("<ConstraintCanvas><Spacer Slot.AutoSize=\"true\" /></ConstraintCanvas></VerticalBox> ); }"),
			TEXT("Ok7"));
		TestTrue(TEXT("well-formed style/slot strings + bool flag compile"), GoodForms.bOk);
		if (GoodForms.bOk)
		{
			TestTrue(TEXT("flag form lowers as FRuiValue(true), not an empty string"),
					 GoodForms.Inl.Contains(TEXT("FRuiValue(true)")));
		}
	}

	// ── §4 markup everywhere: markup-as-value lowers in place (statement positions) ───────
	{
		const FString Source = TEXT(R"UETKX(
component CardStack(Names: TArray<FString>) {
	auto [Count, SetCount] = UseState(0);
	auto Card = (<VerticalBox><TextBlock Text="hi" /></VerticalBox>);
	FRuiNode Bare = <Spacer />;
	TArray<FRuiNode> Rows;
	Rows.Add(<TextBlock Text="row" />);
	return (
		<VerticalBox>
			{ Card }
			{ Bare }
			<Button OnClicked={ SetCount(Count + 1) }>go</Button>
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("CardStack"));
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			AddInfo(FString::Printf(TEXT("diag %s: %s @%d"), *Diag.Code, *Diag.Message, Diag.Offset));
		}
		if (TestTrue(TEXT("markup-as-value compiles"), Out.bOk))
		{
			TestTrue(TEXT("paren value markup lowered"),
					 Out.Inl.Contains(TEXT("auto Card = (")) && Out.Inl.Contains(TEXT("RUI::Slate::VerticalBox(")));
			TestTrue(TEXT("bare `=` value markup lowered"),
					 Out.Inl.Contains(TEXT("FRuiNode Bare = ")) && !Out.Inl.Contains(TEXT("<Spacer />")));
			TestTrue(TEXT("call-argument markup lowered"),
					 Out.Inl.Contains(TEXT("Rows.Add(")) && !Out.Inl.Contains(TEXT("<TextBlock")));
			TestTrue(TEXT("no raw markup leaks into the emitted C++"), !Out.Inl.Contains(TEXT("<VerticalBox")));
			TestEqual(TEXT("hook sig only sees the real hook"), Out.HookSig,
					  FUetkxFileScan::HookSignature({TEXT("UseState")}));
		}

		// value-markup edits must NOT perturb the hook signature (§4.3 HMR protection)
		FUetkxCompileOutput Edited = FUetkxCodegen::CompileSource(
			FString(Source).Replace(TEXT("Text=\"hi\""), TEXT("Text=\"hello there\"")), TEXT("CardStack"));
		if (TestTrue(TEXT("edited variant compiles"), Edited.bOk))
		{
			TestEqual(TEXT("value-markup edit keeps the hook sig"), Edited.HookSig, Out.HookSig);
		}
	}

	// ── §4: the narrowed 0114 (paren-less markup return) + rules of hooks 0013-0016 ───────
	{
		FUetkxCompileOutput BareRet =
			FUetkxCodegen::CompileSource(TEXT("component BareRet {\n\treturn <Spacer />;\n}"), TEXT("BareRet"));
		TestTrue(TEXT("paren-less markup return fails"), !BareRet.bOk);
		TestTrue(TEXT("0114 narrowed to the paren-less return"),
				 BareRet.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX0114"); }));

		auto HasCode = [](const FUetkxCompileOutput& O, const TCHAR* Code)
		{ return O.Diags.ContainsByPredicate([Code](const FUetkxDiag& D) { return D.Code == Code; }); };
		FUetkxCompileOutput HookIf =
			FUetkxCodegen::CompileSource(TEXT("component HookIf(Flag: bool = false) {\n\tif (Flag) {\n\t\tauto [A, "
											  "SetA] = UseState(0);\n\t}\n\treturn "
											  "( <Spacer /> );\n}"),
										 TEXT("HookIf"));
		TestTrue(TEXT("0013 hook in if"), !HookIf.bOk && HasCode(HookIf, TEXT("UETKX0013")));

		FUetkxCompileOutput HookFor =
			FUetkxCodegen::CompileSource(TEXT("component HookFor {\n\tfor (int32 i = 0; i < 3; ++i) {\n\t\tauto [A, "
											  "SetA] = UseState(i);\n\t}\n\treturn "
											  "( <Spacer /> );\n}"),
										 TEXT("HookFor"));
		TestTrue(TEXT("0014 hook in loop"), !HookFor.bOk && HasCode(HookFor, TEXT("UETKX0014")));

		FUetkxCompileOutput HookLambda = FUetkxCodegen::CompileSource(
			TEXT("component HookLambda {\n\tauto Fn = [&]() { auto [A, SetA] = UseState(0); };\n\treturn ( <Spacer /> "
				 ");\n}"),
			TEXT("HookLambda"));
		TestTrue(TEXT("0016 hook in lambda"), !HookLambda.bOk && HasCode(HookLambda, TEXT("UETKX0016")));

		FUetkxCompileOutput HookAttr = FUetkxCodegen::CompileSource(
			TEXT("component HookAttr {\n\treturn ( <TextBlock Text={ FText::AsNumber(UseState(0)) } /> );\n}"),
			TEXT("HookAttr"));
		TestTrue(TEXT("0013 hook in a markup attr expression"), HasCode(HookAttr, TEXT("UETKX0013")));

		FUetkxCompileOutput HookEvent = FUetkxCodegen::CompileSource(
			TEXT("component HookEvent {\n\treturn ( <Button OnClicked={ UseState(0) }>x</Button> );\n}"),
			TEXT("HookEvent"));
		TestTrue(TEXT("0016 hook in an event attr"), HasCode(HookEvent, TEXT("UETKX0016")));

		FUetkxCompileOutput HookDirective = FUetkxCodegen::CompileSource(
			TEXT("component HookDirective {\n\treturn (\n\t\t<VerticalBox>\n\t\t\t@for (int32 i = 0; i < 2; ++i) "
				 "{\n\t\t\t\tauto [A, SetA] = UseState(i);\n\t\t\t\treturn ( <Spacer key={i} /> )\n\t\t\t}\n\t\t"
				 "</VerticalBox>\n\t);\n}"),
			TEXT("HookDirective"));
		TestTrue(TEXT("0014 hook in an @for body"), HasCode(HookDirective, TEXT("UETKX0014")));

		// unconditional top-level hooks (incl. structured bindings + init-captures) stay clean
		FUetkxCompileOutput CleanHooks =
			FUetkxCodegen::CompileSource(TEXT("component CleanHooks {\n\tauto [A, SetA] = "
											  "UseState(0);\n\tUseEffect([Copy = A]() {}, {});\n\treturn ( "
											  "<Spacer /> );\n}"),
										 TEXT("CleanHooks"));
		TestTrue(TEXT("top-level hooks emit no 0013-0016"),
				 CleanHooks.bOk && !HasCode(CleanHooks, TEXT("UETKX0013")) && !HasCode(CleanHooks, TEXT("UETKX0014")) &&
					 !HasCode(CleanHooks, TEXT("UETKX0015")) && !HasCode(CleanHooks, TEXT("UETKX0016")));
	}

	// ── hook-sig stability: same shape -> same sig; different shape -> different ──────────
	{
		const uint32 A = FUetkxFileScan::HookSignature({TEXT("UseState"), TEXT("UseEffect")});
		const uint32 B = FUetkxFileScan::HookSignature({TEXT("UseState"), TEXT("UseEffect")});
		const uint32 C = FUetkxFileScan::HookSignature({TEXT("UseEffect"), TEXT("UseState")});
		TestEqual(TEXT("sig deterministic"), A, B);
		TestNotEqual(TEXT("sig order-sensitive"), A, C);
	}

	// ── exports/privacy (M5, reshaped by FILE_SCOPED_EXPORTS): ONE file namespace for every
	// decl; privates are tree-shaken (no factory) and same-file references stay BARE ─────────
	{
		const FString Src = TEXT("module RowStyle {\n\tinline const int32 Gap = 4;\n}\n")
			TEXT("hook UseLocalCount() -> int32 {\n\treturn 0;\n}\n")
				TEXT("component Row {\n\treturn ( <Spacer /> );\n}\n") TEXT("export component Panel {\n")
					TEXT("\tint32 Pad = RowStyle::Gap;\n") TEXT("\tauto V = UseLocalCount();\n")
						TEXT("\treturn ( <VerticalBox> <Row /> </VerticalBox> );\n}\n");
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Src, TEXT("Panel"));
		if (TestTrue(TEXT("privacy sample compiles"), Out.bOk))
		{
			// exported component: FQN named factory.
			TestTrue(
				TEXT("exported component registers a named factory under the FQN"),
				Out.Inl.Contains(TEXT(
					"RUI::RegisterNamedFactory(FName(TEXT(\"RuiUetkx_Panel::Panel\")), []() { return Panel(); })")));
			TestTrue(TEXT("only exported decls in the export surface"),
					 Out.ExportedNames.Num() == 1 && Out.ExportedNames[0] == TEXT("Panel"));
			// EVERYTHING wraps in the file namespace; RuiPriv_ is retired.
			TestTrue(TEXT("file namespace wraps the decls"), Out.Inl.Contains(TEXT("namespace RuiUetkx_Panel")));
			TestFalse(TEXT("RuiPriv_ detail namespace is retired"), Out.Inl.Contains(TEXT("RuiPriv_")));
			TestTrue(TEXT("private component NOT globally registered"),
					 !Out.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"Row\"))")) &&
						 !Out.Inl.Contains(TEXT("::Row\")), []()")));
			TestTrue(TEXT("private component id is FILE-QUALIFIED"),
					 Out.Inl.Contains(TEXT("RegisterComponentId((void*)&Row_UetkxImpl,")) &&
						 Out.Inl.Contains(TEXT("FName(TEXT(\"RuiUetkx_Panel::Row\")))")));
			// same-file references stay BARE — they share the file namespace.
			TestTrue(TEXT("same-file component tag stays bare"),
					 Out.Inl.Contains(TEXT("FRowUetkxProps P;")) && Out.Inl.Contains(TEXT("return Row(MoveTemp(P)")));
			TestTrue(TEXT("same-file hook call stays bare"), Out.Inl.Contains(TEXT("auto V = UseLocalCount(Ctx);")));
			TestTrue(TEXT("same-file module qual stays bare"), Out.Inl.Contains(TEXT("int32 Pad = RowStyle::Gap;")));
		}
	}

	// ── two-phase emit shape (M6): phase guards + defaults on the DECL wrapper only ───────────────
	{
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(
			TEXT("export component TwoPhase(Title: FText) {\n\treturn ( <Spacer /> );\n}\n"), TEXT("TwoPhase"));
		if (TestTrue(TEXT("two-phase sample compiles"), Out.bOk))
		{
			TestTrue(TEXT("has the decl-phase guard"), Out.Inl.Contains(TEXT("#if defined(RUI_UETKX_DECL_PHASE)")));
			TestTrue(TEXT("has the body-phase #else"), Out.Inl.Contains(TEXT("#else")));
			TestTrue(TEXT("has #endif"), Out.Inl.Contains(TEXT("#endif")));
			// The DECL phase carries the complete struct + a defaulted forward declaration.
			TestTrue(TEXT("decl phase has the complete props struct"),
					 Out.Inl.Contains(TEXT("struct FTwoPhaseUetkxProps final : public FRuiPropsBase")));
			TestTrue(TEXT("decl-phase wrapper is a DEFAULTED forward declaration"),
					 Out.Inl.Contains(
						 TEXT("inline FRuiNode TwoPhase(FTwoPhaseUetkxProps InProps = FTwoPhaseUetkxProps(), "
							  "TArray<FRuiNode> InChildren = TArray<FRuiNode>(), FRuiKey InKey = FRuiKey());")));
			// The BODY phase repeats the signature WITHOUT defaults (C++ redefinition rule).
			TestTrue(TEXT("body-phase wrapper definition drops the defaults"),
					 Out.Inl.Contains(TEXT("inline FRuiNode TwoPhase(FTwoPhaseUetkxProps InProps, TArray<FRuiNode> "
										   "InChildren, FRuiKey InKey)\n{")));
			TestFalse(TEXT("no defaulted wrapper DEFINITION (would be a redefinition error)"),
					  Out.Inl.Contains(TEXT("FRuiKey InKey = FRuiKey())\n{")));
			// The impl lives in the body phase; the struct does NOT repeat there.
			const int32 Split = Out.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("impl is after #else (body phase)"),
					 Split >= 0 && Out.Inl.Find(TEXT("TwoPhase_UetkxImpl")) > Split);
		}
	}

	// ── ES-modules M2: value/util emission + the alias plane (U-03/U-04) ─────────────────────────
	{
		// Typed + inferred value exports: DECL-phase-only inline FUNCTIONS returning by value
		// (TB-15: a Live Coding patch replaces the CODE that produces the value — a global's
		// initializer never re-runs on patch, which made style-companion edits invisible to HMR).
		const FUetkxCompileOutput Vals =
			FUetkxCodegen::CompileSource(TEXT("export FLinearColor Cool = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);\n")
											 TEXT("export Accent = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);\n"),
										 TEXT("Palette2"));
		if (TestTrue(TEXT("value exports compile"), Vals.bOk))
		{
			TestTrue(TEXT("typed value emits an inline FUNCTION (TB-15 — HMR-patchable code, not data)"),
					 Vals.Inl.Contains(TEXT("inline FLinearColor Cool()")));
			TestTrue(TEXT("inferred value emits an inline auto FUNCTION"),
					 Vals.Inl.Contains(TEXT("inline auto Accent()")));
			const int32 Split = Vals.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("values are DECL-phase-only (before #else)"),
					 Split >= 0 && Vals.Inl.Find(TEXT("Cool")) < Split &&
						 Vals.Inl.Find(TEXT("Cool"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) < Split);
			TestTrue(TEXT("exported values join the export ledger"),
					 Vals.ExportedNames.Contains(TEXT("Cool")) && Vals.ExportedNames.Contains(TEXT("Accent")));
			TestTrue(TEXT("a value/util-only file is a support file"), Vals.bSupportFile);
		}

		// Util: fwd-decl (DECL phase) + definition (BODY phase), no Ctx injection into the signature.
		const FUetkxCompileOutput Util = FUetkxCodegen::CompileSource(
			TEXT("export FString FormatScore(int32 Score) {\n\treturn FString::FromInt(Score);\n}\n"),
			TEXT("ScoreFmt"));
		if (TestTrue(TEXT("util compiles"), Util.bOk))
		{
			const int32 Split = Util.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("util fwd-decl in the DECL phase"),
					 Split >= 0 && Util.Inl.Find(TEXT("inline FString FormatScore(int32 Score);")) < Split);
			TestTrue(TEXT("util definition in the BODY phase"),
					 Util.Inl.Find(TEXT("inline FString FormatScore(int32 Score)\n{")) > Split);
			TestFalse(TEXT("no Ctx in a util signature"), Util.Inl.Contains(TEXT("FormatScore(FRuiContext&")));
		}

		// Private value + util: inside the file namespace with everything else; same-file
		// references stay BARE (the value still lowers as a CALL — TB-15).
		const FUetkxCompileOutput Priv = FUetkxCodegen::CompileSource(
			TEXT("FLinearColor RowTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);\n")
				TEXT("FString Pad(FString S) {\n\treturn S;\n}\n") TEXT("export FRuiNode Panel2() {\n")
					TEXT("\tauto T = RowTint;\n\tauto S = Pad(FString());\n\treturn ( <Spacer /> );\n}\n"),
			TEXT("Panel2"));
		if (TestTrue(TEXT("private value/util sample compiles"), Priv.bOk))
		{
			TestTrue(TEXT("file namespace wraps the decls"), Priv.Inl.Contains(TEXT("namespace RuiUetkx_Panel2")));
			TestTrue(TEXT("private value reference stays bare, as a CALL (TB-15)"),
					 Priv.Inl.Contains(TEXT("auto T = RowTint();")));
			TestTrue(TEXT("private util call stays bare"), Priv.Inl.Contains(TEXT("auto S = Pad(FString());")));
			TestFalse(TEXT("RuiPriv_ is retired"), Priv.Inl.Contains(TEXT("RuiPriv_")));
			TestTrue(TEXT("exported list excludes privates"),
					 Priv.ExportedNames.Num() == 1 && Priv.ExportedNames[0] == TEXT("Panel2"));
		}

		// ES parity (family 0.9.1 field wave): a DEFAULT-exported declaration is PUBLIC — the
		// default importer rewrites to its qualified target (FS-03); like every decl it lives in
		// the file namespace. It stays name-import-private (the resolver's export table never
		// marks it — `import { FmtD }` still 2301s; pinned in the Resolve suite).
		const FUetkxCompileOutput DefPriv = FUetkxCodegen::CompileSource(
			TEXT("FString FmtD(int32 S) {\n\treturn FString::FromInt(S);\n}\n")
				TEXT("export FRuiNode Panel3() {\n\tauto S = FmtD(1);\n\treturn ( <Spacer /> );\n}\n")
					TEXT("export default FmtD;\n"),
			TEXT("Panel3"));
		if (TestTrue(TEXT("default-exported private util compiles"), DefPriv.bOk))
		{
			TestTrue(TEXT("default-exported util fwd-declares in the file namespace"),
					 DefPriv.Inl.Contains(TEXT("inline FString FmtD(int32 S);")) &&
						 DefPriv.Inl.Contains(TEXT("namespace RuiUetkx_Panel3")));
			TestFalse(TEXT("RuiPriv_ is retired"), DefPriv.Inl.Contains(TEXT("RuiPriv_")));
			TestTrue(TEXT("same-file references stay bare"), DefPriv.Inl.Contains(TEXT("auto S = FmtD(1);")));
			TestTrue(TEXT("default name joins the export surface"), DefPriv.ExportedNames.Contains(TEXT("FmtD")));
		}

		// New-form component + hook feed the EXISTING emitters (props struct + registrations).
		const FUetkxCompileOutput NewForm = FUetkxCodegen::CompileSource(
			TEXT("export int32 UseTick(int32 Start) {\n\treturn Start;\n}\n")
				TEXT("export FRuiNode Chip2(FString Label, int32 Count = 0) {\n")
					TEXT("\tauto V = UseTick(1);\n\treturn ( <TextBlock Text={ FText::FromString(Label) } /> );\n}\n"),
			TEXT("Chip2"));
		if (TestTrue(TEXT("new-form component + hook compile"), NewForm.bOk))
		{
			TestTrue(TEXT("props struct emitted"), NewForm.Inl.Contains(TEXT("struct FChip2UetkxProps")));
			TestTrue(TEXT("C++-native param -> props field with default"),
					 NewForm.Inl.Contains(TEXT("int32 Count = 0;")) && NewForm.Inl.Contains(TEXT("FString Label")));
			TestTrue(TEXT("new-form hook takes Ctx first"),
					 NewForm.Inl.Contains(TEXT("inline int32 UseTick(FRuiContext& Ctx, int32 Start)")));
			TestTrue(TEXT("hook call site Ctx-injected"), NewForm.Inl.Contains(TEXT("UseTick(Ctx, 1)")));
			TestTrue(TEXT("named factory registered under the FQN"),
					 NewForm.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"RuiUetkx_Chip2::Chip2\"))")));
		}

		// The import-binding plane (FS-03): every imported reference — renamed, bare, star, or
		// default — rewrites to the TARGET FILE's qualified spelling (imported values gain their
		// call form, TB-15). Qualification needs the target surface, so a stub resolver supplies
		// Palette2/StatusChip/Hooks2.
		const FUetkxCodegenTestResolver AliasResolver = MakeAliasResolver();
		const FUetkxCompileOutput Alias = FUetkxCodegen::CompileSource(
			TEXT("import { StatusChip as Chip } from \"./StatusChip\"\n")
				TEXT("import { UseCounter as UseTick } from \"./Hooks2\"\n")
					TEXT("import { Cool as Primary } from \"./Palette2\"\n")
						TEXT("import * as Palette from \"./Palette2\"\n") TEXT("export FRuiNode AliasUser() {\n")
							TEXT("\tauto A = UseTick(1);\n\tauto B = Primary;\n\tauto C = Palette::Accent;\n")
								TEXT("\treturn ( <Chip /> );\n}\n"),
			TEXT("AliasUser"), FString(), &AliasResolver);
		if (TestTrue(TEXT("alias sample compiles"), Alias.bOk))
		{
			TestTrue(TEXT("renamed tag emits the target props+factory, FILE-QUALIFIED"),
					 Alias.Inl.Contains(TEXT("RuiUetkx_StatusChip::FStatusChipUetkxProps")) &&
						 Alias.Inl.Contains(TEXT("RuiUetkx_StatusChip::StatusChip(")));
			TestFalse(TEXT("the local tag alias never reaches the C++"), Alias.Inl.Contains(TEXT("FChipUetkxProps")));
			TestTrue(TEXT("renamed hook call rewrites to the qualified target + Ctx"),
					 Alias.Inl.Contains(TEXT("RuiUetkx_Hooks2::UseCounter(Ctx, 1)")));
			TestTrue(TEXT("renamed value reference rewrites to a qualified CALL"),
					 Alias.Inl.Contains(TEXT("auto B = RuiUetkx_Palette2::Cool();")));
			TestTrue(TEXT("star qual maps to the target file namespace"),
					 Alias.Inl.Contains(TEXT("auto C = RuiUetkx_Palette2::Accent();")));
			TestTrue(TEXT("Uses records the TARGET component"), Alias.Uses.Contains(TEXT("StatusChip")));
			TestFalse(TEXT("Uses does not record the alias"), Alias.Uses.Contains(TEXT("Chip")));
		}

		// ES COMBINED forms at the binding plane: ONE declaration carrying default + named/star —
		// every part must land even though the declaration also carries a default binding
		// (exclusive branching dropped them; Unity 0.9.1 parity).
		const FUetkxCompileOutput Combined = FUetkxCodegen::CompileSource(
			TEXT("import Def, { Cool as Primary } from \"./Palette2\"\n")
				TEXT("import Def2, * as Palette from \"./Palette2\"\n") TEXT("export FRuiNode CombinedUser() {\n")
					TEXT("\tauto B = Primary;\n\tauto C = Palette::Accent;\n\tauto D = Def2(1);\n")
						TEXT("\treturn ( <Def /> );\n}\n"),
			TEXT("CombinedUser"), FString(), &AliasResolver);
		if (TestTrue(TEXT("combined alias sample compiles"), Combined.bOk))
		{
			TestTrue(TEXT("combined named rename rewrites beside the default"),
					 Combined.Inl.Contains(TEXT("auto B = RuiUetkx_Palette2::Cool();")));
			TestTrue(TEXT("combined star qual maps beside the default"),
					 Combined.Inl.Contains(TEXT("auto C = RuiUetkx_Palette2::Accent();")));
			TestTrue(TEXT("default binding rewrites to the qualified default target"),
					 Combined.Inl.Contains(TEXT("auto D = RuiUetkx_Palette2::PalCard(1);")));
			TestTrue(TEXT("default TAG emits the qualified target"),
					 Combined.Inl.Contains(TEXT("RuiUetkx_Palette2::PalCard(MoveTemp(P)")));
		}

		// IMPORT-3 regression at the BINDING plane (M7 bughunt): a ternary's SECOND arm sits after
		// a lone `:` — the scan-back must not read it as a `::` scope qual, or the binding
		// survives unrewritten into broken C++.
		const FUetkxCompileOutput Tern = FUetkxCodegen::CompileSource(
			TEXT("import * as Pal from \"./Palette2\"\n") TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT(
				"export FRuiNode TernAlias() {\n") TEXT("\tconst FLinearColor T = true ? Pal::Accent : Pal::Cool;\n")
				TEXT("\tconst FLinearColor U = false ? Primary : Primary;\n") TEXT("\treturn ( <Spacer /> );\n}\n"),
			TEXT("TernAlias"), FString(), &AliasResolver);
		if (TestTrue(TEXT("ternary alias sample compiles"), Tern.bOk))
		{
			TestTrue(TEXT("ternary second arm maps the star qual"),
					 Tern.Inl.Contains(TEXT("true ? RuiUetkx_Palette2::Accent() : RuiUetkx_Palette2::Cool();")));
			TestTrue(TEXT("ternary second arm rewrites the rename binding"),
					 Tern.Inl.Contains(TEXT("false ? RuiUetkx_Palette2::Cool() : RuiUetkx_Palette2::Cool();")));
			TestFalse(TEXT("no local alias qual survives"), Tern.Inl.Contains(TEXT("Pal::")));
		}

		// FS-04 (supersedes TD-026's split): runtime identity is FILE-QUALIFIED for EVERY
		// component — two files' `Row`s (private) AND their exported wrappers get distinct
		// RegisterComponentId keys; neither private registers a named factory (tree-shaken).
		{
			const FString PairSrc = TEXT("FRuiNode Row() {\n\treturn ( <Spacer /> );\n}\n")
				TEXT("export FRuiNode PAIRNAME() {\n\treturn ( <VerticalBox> <Row /> </VerticalBox> );\n}\n");
			const FUetkxCompileOutput A =
				FUetkxCodegen::CompileSource(PairSrc.Replace(TEXT("PAIRNAME"), TEXT("PrivPairA")), TEXT("PrivPairA"));
			const FUetkxCompileOutput B =
				FUetkxCodegen::CompileSource(PairSrc.Replace(TEXT("PAIRNAME"), TEXT("PrivPairB")), TEXT("PrivPairB"));
			if (TestTrue(TEXT("PrivPair sources compile"), A.bOk && B.bOk))
			{
				TestTrue(TEXT("A's private Row keys RuiUetkx_PrivPairA::Row"),
						 A.Inl.Contains(TEXT("RegisterComponentId((void*)&Row_UetkxImpl,")) &&
							 A.Inl.Contains(TEXT("FName(TEXT(\"RuiUetkx_PrivPairA::Row\")))")));
				TestTrue(TEXT("B's private Row keys RuiUetkx_PrivPairB::Row"),
						 B.Inl.Contains(TEXT("RegisterComponentId((void*)&Row_UetkxImpl,")) &&
							 B.Inl.Contains(TEXT("FName(TEXT(\"RuiUetkx_PrivPairB::Row\")))")));
				TestTrue(TEXT("exported components carry the FILE-QUALIFIED runtime id too (FS-04)"),
						 A.Inl.Contains(TEXT("RegisterComponentId((void*)&PrivPairA_UetkxImpl,")) &&
							 A.Inl.Contains(TEXT("FName(TEXT(\"RuiUetkx_PrivPairA::PrivPairA\")))")));
				TestFalse(TEXT("no bare-name id for a private component"),
						  A.Inl.Contains(TEXT(", FName(TEXT(\"Row\")))")));
				TestFalse(TEXT("neither private registers a named factory"),
						  A.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"Row\"))")) ||
							  B.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"Row\"))")) ||
							  A.Inl.Contains(TEXT("::Row\")), []()")) || B.Inl.Contains(TEXT("::Row\")), []()")));
			}
		}

		// FILE_SCOPED_EXPORTS headline (FS-01..FS-04): TWO files may export the SAME names — each
		// emits into its own namespace with its own FQN identities; an importer binding both via
		// `as` aliases addresses each unambiguously (the ExpPair contract fixtures pin the same
		// truth against the golden harness).
		{
			FUetkxCodegenTestResolver PairResolver;
			auto Decl = [](EUetkxDeclKind K)
			{
				FUetkxTargetDecl D;
				D.Kind = K;
				D.bExported = true;
				return D;
			};
			TMap<FString, FUetkxTargetDecl>& FA = PairResolver.Files.Add(TEXT("ExpPairA"));
			FA.Add(TEXT("Widget"), Decl(EUetkxDeclKind::Component));
			FA.Add(TEXT("Accent"), Decl(EUetkxDeclKind::Value));
			TMap<FString, FUetkxTargetDecl>& FB = PairResolver.Files.Add(TEXT("ExpPairB"));
			FB.Add(TEXT("Widget"), Decl(EUetkxDeclKind::Component));
			FB.Add(TEXT("Accent"), Decl(EUetkxDeclKind::Value));
			const FUetkxCompileOutput Use = FUetkxCodegen::CompileSource(
				TEXT("import { Widget, Accent } from \"./ExpPairA\"\n")
					TEXT("import { Widget as WidgetB, Accent as AccentB } from \"./ExpPairB\"\n")
						TEXT("export FRuiNode PairUse() {\n")
							TEXT("\tconst FLinearColor Mix = FLinearColor(Accent.R, AccentB.B, 0.0f, 1.0f);\n")
								TEXT("\treturn ( <VerticalBox> <Widget /> <WidgetB /> <Border "
									 "BorderBackgroundColor={ Mix }><Spacer /></Border> </VerticalBox> );\n}\n"),
				TEXT("PairUse"), FString(), &PairResolver);
			if (TestTrue(TEXT("same-name pair importer compiles"), Use.bOk))
			{
				TestTrue(TEXT("A's Widget addresses A's namespace"),
						 Use.Inl.Contains(TEXT("RuiUetkx_ExpPairA::Widget(MoveTemp(P)")));
				TestTrue(TEXT("B's Widget addresses B's namespace"),
						 Use.Inl.Contains(TEXT("RuiUetkx_ExpPairB::Widget(MoveTemp(P)")));
				TestTrue(TEXT("A's Accent value-call qualifies to A"),
						 Use.Inl.Contains(TEXT("RuiUetkx_ExpPairA::Accent().R")));
				TestTrue(TEXT("B's Accent value-call qualifies to B"),
						 Use.Inl.Contains(TEXT("RuiUetkx_ExpPairB::Accent().B")));
			}
		}

		// TD-034 #1 (N4): the scope tracker — a LOCAL shadowing an alias/private name keeps its
		// bare spelling inside its scope; outside the scope (or before the declaration) the
		// rewrite still fires. Every recognized pattern is pinned here (params, `Type Name =`,
		// `auto [A, B]`, inner-brace expiry).
		{
			// A local `FLinearColor Primary = …` shadows the rename binding: references AFTER the
			// declaration stay bare; the reference BEFORE it still rewrites to the qualified target.
			const FUetkxCodegenTestResolver ShadowResolver = MakeAliasResolver();
			const FUetkxCompileOutput Shadow = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowVal() {\n")
					TEXT("\tauto Before = Primary;\n")
						TEXT("\tFLinearColor Primary = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);\n")
							TEXT("\tauto After = Primary;\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowVal"), FString(), &ShadowResolver);
			if (TestTrue(TEXT("alias-shadow sample compiles"), Shadow.bOk))
			{
				TestTrue(TEXT("pre-declaration reference still rewrites (qualified value call)"),
						 Shadow.Inl.Contains(TEXT("auto Before = RuiUetkx_Palette2::Cool();")));
				TestTrue(TEXT("post-declaration reference keeps the local"),
						 Shadow.Inl.Contains(TEXT("auto After = Primary;")));
			}

			// A local shadowing a same-file PRIVATE value keeps its bare no-call spelling; the
			// un-shadowed reference lowers as the bare CALL (TB-15; same file ⇒ no qualification).
			const FUetkxCompileOutput PrivShadow = FUetkxCodegen::CompileSource(
				TEXT("FLinearColor RowTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);\n")
					TEXT("FString Pad(FString S) {\n\treturn S;\n}\n") TEXT("export FRuiNode ShadowPriv() {\n")
						TEXT("\tif (true) {\n\t\tFLinearColor RowTint = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);\n")
							TEXT("\t\tauto Inner = RowTint;\n\t}\n") TEXT(
								"\tauto Outer = RowTint;\n\tauto S = Pad(FString());\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowPriv"));
			if (TestTrue(TEXT("private-shadow sample compiles"), PrivShadow.bOk))
			{
				TestTrue(TEXT("inner-scope shadow keeps the bare local (no call)"),
						 PrivShadow.Inl.Contains(TEXT("auto Inner = RowTint;")));
				TestTrue(TEXT("outer reference lowers as the bare CALL after the scope closes (TB-15)"),
						 PrivShadow.Inl.Contains(TEXT("auto Outer = RowTint();")));
				TestFalse(TEXT("RuiPriv_ is retired"), PrivShadow.Inl.Contains(TEXT("RuiPriv_")));
			}

			// A component PARAM shadowing a binding name suppresses the rewrite for the whole body
			// (a sibling component in the same file keeps the import genuinely used); a structured
			// binding (`auto [Primary, SetPrimary] = …`) does too.
			const FUetkxCompileOutput ParamShadow = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode UsesIt() {\n")
					TEXT("\tauto K = Primary;\n\treturn ( <Spacer /> );\n}\n")
						TEXT("export FRuiNode ShadowParam(FLinearColor Primary) {\n")
							TEXT("\tauto V = Primary;\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowParam"), FString(), &ShadowResolver);
			if (TestTrue(TEXT("param-shadow sample compiles"), ParamShadow.bOk))
			{
				TestTrue(TEXT("param shadow keeps the bare spelling"),
						 ParamShadow.Inl.Contains(TEXT("auto V = Primary;")));
				TestFalse(TEXT("no rewrite to the target inside the shadowed body"),
						  ParamShadow.Inl.Contains(TEXT("auto V = RuiUetkx_Palette2::Cool();")));
				TestTrue(TEXT("the un-shadowed sibling still rewrites"),
						 ParamShadow.Inl.Contains(TEXT("auto K = RuiUetkx_Palette2::Cool();")));
			}
			const FUetkxCompileOutput BindShadow = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowBind() {\n")
					TEXT("\tauto Use0 = Primary;\n") TEXT("\tauto [Primary, SetPrimary] = UseState(FLinearColor());\n")
						TEXT("\tauto V = Primary;\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowBind"), FString(), &ShadowResolver);
			if (TestTrue(TEXT("binding-shadow sample compiles"), BindShadow.bOk))
			{
				TestTrue(TEXT("structured-binding shadow keeps the bare spelling"),
						 BindShadow.Inl.Contains(TEXT("auto V = Primary;")));
				TestTrue(TEXT("the pre-binding reference still rewrites"),
						 BindShadow.Inl.Contains(TEXT("auto Use0 = RuiUetkx_Palette2::Cool();")));
			}

			// A local callable named like a USER hook (`auto UseWobble = …;`) is NOT Ctx-injected.
			const FUetkxCompileOutput HookShadow = FUetkxCodegen::CompileSource(
				TEXT("export FRuiNode ShadowHook() {\n") TEXT("\tauto UseWobble = [](int32 A) { return A; };\n")
					TEXT("\tauto W = UseWobble(2);\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowHook"));
			if (TestTrue(TEXT("hook-shadow sample compiles"), HookShadow.bOk))
			{
				TestTrue(TEXT("local callable keeps its plain call"), HookShadow.Inl.Contains(TEXT("UseWobble(2)")));
				TestFalse(TEXT("no Ctx injection into the local call"),
						  HookShadow.Inl.Contains(TEXT("UseWobble(Ctx, 2)")));
			}
		}

		// TD-034 #1 (N4 audit): the shadow tracker must hold inside MARKUP EXPRESSIONS and across
		// value-markup fragmentation — a setup local is live in attr exprs (the impl body scope),
		// and a declaration BEFORE a value-markup range is still visible after it.
		{
			// A setup local shadowing a rename binding, referenced from an ATTR expression: the
			// attr must emit the LOCAL spelling (rewriting to the target would silently read the
			// imported value instead of the local). `Use0` keeps the import genuinely used.
			const FUetkxCodegenTestResolver AttrResolver = MakeAliasResolver();
			const FUetkxCompileOutput AttrShadow = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowAttr() {\n")
					TEXT("\tauto Use0 = Primary;\n")
						TEXT("\tFLinearColor Primary = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);\n")
							TEXT("\treturn ( <Border BorderBackgroundColor={ Primary } /> );\n}\n"),
				TEXT("ShadowAttr"), FString(), &AttrResolver);
			if (TestTrue(TEXT("attr-shadow sample compiles"), AttrShadow.bOk))
			{
				TestTrue(TEXT("attr expr keeps the shadowing local"),
						 AttrShadow.Inl.Contains(TEXT("FRuiValue(Primary)")) ||
							 AttrShadow.Inl.Contains(TEXT("( Primary )")) ||
							 AttrShadow.Inl.Contains(TEXT("(Primary)")));
				TestFalse(TEXT("attr expr does NOT rewrite to the binding target"),
						  AttrShadow.Inl.Contains(TEXT("FRuiValue(RuiUetkx_Palette2::Cool())")));
			}

			// Value-markup fragmentation: a local declared BEFORE an embedded markup value stays
			// visible in the setup code AFTER it (the emitter splits the region around the range).
			const FUetkxCompileOutput FragShadow = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowFrag() {\n")
					TEXT("\tauto Use0 = Primary;\n")
						TEXT("\tFLinearColor Primary = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);\n")
							TEXT("\tFRuiNode Chip = <Spacer />;\n") TEXT("\tauto After = Primary;\n")
								TEXT("\treturn ( <VerticalBox>{ Chip }</VerticalBox> );\n}\n"),
				TEXT("ShadowFrag"), FString(), &AttrResolver);
			if (TestTrue(TEXT("fragmented-shadow sample compiles"), FragShadow.bOk))
			{
				TestTrue(TEXT("post-range reference keeps the local"),
						 FragShadow.Inl.Contains(TEXT("auto After = Primary;")));
			}
		}

		// N4 audit round 2: range-for vars, lambda params, directive-frame locals, and the
		// comma rule — every recognized tracker pattern pinned at the EMISSION plane.
		{
			// A range-for variable shadowing the binding keeps its bare spelling in the loop body
			// (`Use0` keeps the import used under strict resolution).
			const FUetkxCodegenTestResolver TrackerResolver = MakeAliasResolver();
			const FUetkxCompileOutput RangeFor = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowRange() {\n")
					TEXT("\tauto Use0 = Primary;\n\tTArray<FLinearColor> Tints;\n")
						TEXT("\tfor (const FLinearColor& Primary : Tints) {\n\t\tauto V = Primary;\n\t}\n")
							TEXT("\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowRange"), FString(), &TrackerResolver);
			if (TestTrue(TEXT("range-for shadow compiles"), RangeFor.bOk))
			{
				TestTrue(TEXT("range-for var stays bare"), RangeFor.Inl.Contains(TEXT("auto V = Primary;")));
			}

			// A lambda parameter shadowing the binding keeps its bare spelling in the lambda body.
			const FUetkxCompileOutput Lambda = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowLambda() {\n")
					TEXT("\tauto Use0 = Primary;\n")
						TEXT("\tauto Fn = [](const FLinearColor& Primary) { return Primary.R; };\n")
							TEXT("\treturn ( <Spacer /> );\n}\n"),
				TEXT("ShadowLambda"), FString(), &TrackerResolver);
			if (TestTrue(TEXT("lambda-param shadow compiles"), Lambda.bOk))
			{
				TestTrue(TEXT("lambda param stays bare"), Lambda.Inl.Contains(TEXT("return Primary.R;")));
			}

			// An @for loop var shadowing the binding stays bare inside a NESTED attr expression.
			const FUetkxCompileOutput LoopAttr = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode ShadowLoopAttr() {\n")
					TEXT("\tauto Use0 = Primary;\n\treturn (\n\t\t<VerticalBox>\n")
						TEXT("\t\t\t@for (int32 Primary = 0; Primary < 3; ++Primary) {\n")
							TEXT("\t\t\t\treturn ( <TextBlock Text={ FText::AsNumber(Primary) } /> )\n\t\t\t}\n")
								TEXT("\t\t</VerticalBox>\n\t);\n}\n"),
				TEXT("ShadowLoopAttr"), FString(), &TrackerResolver);
			if (TestTrue(TEXT("loop-var attr shadow compiles"), LoopAttr.bOk))
			{
				TestTrue(TEXT("nested attr keeps the loop var"),
						 LoopAttr.Inl.Contains(TEXT("FText::AsNumber(Primary)")));
			}

			// The comma RESETS type-ish (audit: `MakeTuple(1, Primary)` must still rewrite its
			// second argument — keeping type-ish through `,` mis-declared it as a local).
			const FUetkxCompileOutput Comma = FUetkxCodegen::CompileSource(
				TEXT("import { Cool as Primary } from \"./Palette2\"\n") TEXT("export FRuiNode CommaArg() {\n")
					TEXT("\tauto T = MakeTuple(1, Primary);\n\treturn ( <Spacer /> );\n}\n"),
				TEXT("CommaArg"), FString(), &TrackerResolver);
			if (TestTrue(TEXT("comma-arg sample compiles"), Comma.bOk))
			{
				TestTrue(TEXT("second argument rewrites to the qualified target"),
						 Comma.Inl.Contains(TEXT("MakeTuple(1, RuiUetkx_Palette2::Cool())")));
			}
		}

		// FS-01: the namespace derivation table — the ONE rule every caller (codegen, driver,
		// preview, LSP mirror) derives from. Sanitization edges: companion dots, digits, C++
		// keywords, non-identifier characters, empty ProjRel fallback.
		{
			TestEqual(TEXT("fixture fallback (empty ProjRel)"),
					  FUetkxCodegen::FileNamespaceFor(FString(), TEXT("Counter")), TEXT("RuiUetkx_Counter"));
			TestEqual(TEXT("project-relative path derives the FLAT single-identifier namespace"),
					  FUetkxCodegen::FileNamespaceFor(TEXT("Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.uetkx"),
													  TEXT("SimpleCounter")),
					  TEXT("RuiUetkx_Source_RuiDemo_Screens_SimpleCounter_SimpleCounter"));
			TestEqual(TEXT("companion dots fold to underscores"),
					  FUetkxCodegen::FileNamespaceFor(
						  TEXT("Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.style.uetkx"), TEXT("")),
					  TEXT("RuiUetkx_Source_RuiDemo_Screens_SimpleCounter_SimpleCounter_style"));
			TestEqual(TEXT("leading digit gets an underscore prefix"),
					  FUetkxCodegen::FileNamespaceFor(TEXT("Source/3D/Hud.uetkx"), TEXT("")),
					  TEXT("RuiUetkx_Source__3D_Hud"));
			TestEqual(TEXT("a C++ keyword segment gets an underscore prefix"),
					  FUetkxCodegen::FileNamespaceFor(TEXT("Source/template/Card.uetkx"), TEXT("")),
					  TEXT("RuiUetkx_Source__template_Card"));
			TestEqual(TEXT("non-identifier characters fold to underscores"),
					  FUetkxCodegen::FileNamespaceFor(TEXT("Source/My Game/Hud-Main.uetkx"), TEXT("")),
					  TEXT("RuiUetkx_Source_My_Game_Hud_Main"));
		}

		// Mixed 5-kind file: source order preserved within each phase region.
		const FUetkxCompileOutput Mixed = FUetkxCodegen::CompileSource(
			TEXT("export FRuiNode Widget5() {\n\treturn ( <Spacer /> );\n}\n")
				TEXT("export int32 UseFive(int32 A) {\n\treturn A;\n}\n")
					TEXT("export FLinearColor Five = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
						TEXT("export FString FmtFive(int32 S) {\n\treturn FString::FromInt(S);\n}\n")
							TEXT("export module FiveStyles { inline const float P5 = 5.0f; }\n"),
			TEXT("Widget5"));
		if (TestTrue(TEXT("mixed 5-kind file compiles"), Mixed.bOk))
		{
			TestTrue(TEXT("all five kinds present"),
					 Mixed.Inl.Contains(TEXT("FWidget5UetkxProps")) && Mixed.Inl.Contains(TEXT("UseFive")) &&
						 Mixed.Inl.Contains(TEXT("inline FLinearColor Five()")) &&
						 Mixed.Inl.Contains(TEXT("FmtFive")) && Mixed.Inl.Contains(TEXT("namespace FiveStyles")));
			TestTrue(TEXT("exported ledger carries all five"),
					 Mixed.ExportedNames.Contains(TEXT("Widget5")) && Mixed.ExportedNames.Contains(TEXT("UseFive")) &&
						 Mixed.ExportedNames.Contains(TEXT("Five")) && Mixed.ExportedNames.Contains(TEXT("FmtFive")) &&
						 Mixed.ExportedNames.Contains(TEXT("FiveStyles")));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
