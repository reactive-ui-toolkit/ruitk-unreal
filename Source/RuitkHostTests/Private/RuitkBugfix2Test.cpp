// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Bugfix2.* — regression tests locking the fixes from the SECOND adversarial bug hunt
// (plans/archive/BUGHUNT_2026-07-12_round2.md). Each asserts the previously-uncovered case: the neutral
// copyright banner (CG-1), a mis-cased host tag (CG-3), operator-in-default param splitting (SCAN-1),
// a comment inside an import list (SCAN-2), a hook missing its body (SCAN-3), theme-token resolution
// (STYLE-1) and space-after-comma vectors (STYLE-2), NumericEntryBox clamping (IW-1), a POP navigation
// honoring a blocker (RECON-2), and the would-be-label reverse-staleness key (DRV-2 / IMPORT-1).

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkNode.h"
#include "RuitkNumericEntryBox.h"
#include "RuitkRoot.h"
#include "RuitkRouter.h"
#include "RuitkStyle.h"
#include "RuitkTypes.h"
#include "UetkxCodegen.h"
#include "UetkxFileScan.h"
#include "UetkxResolve.h"

#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Bugfix2Test
{
	static bool DiagsContain(const TArray<FUetkxDiag>& Diags, const TCHAR* Code)
	{
		for (const FUetkxDiag& D : Diags)
		{
			if (D.Code == Code)
			{
				return true;
			}
		}
		return false;
	}
} // namespace Bugfix2Test

// ── CG-1: the D-32(a) context-aware generated-code banner (seller vs neutral) ───────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2CopyrightTest, "Ruitk.Bugfix2.CopyrightBanner",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2CopyrightTest::RunTest(const FString&)
{
	const FString Src = TEXT("component Foo {\n\treturn (<TextBlock Text=\"hi\"/>);\n}\n");
	const FUetkxCompileOutput Seller =
		FUetkxCodegen::CompileSource(Src, TEXT("Foo"), FString(), nullptr, TOptional<bool>(true));
	const FUetkxCompileOutput Neutral =
		FUetkxCodegen::CompileSource(Src, TEXT("Foo"), FString(), nullptr, TOptional<bool>(false));
	TestTrue(TEXT("seller override stamps the seller copyright"), Seller.Inl.Contains(TEXT("Yaniv Kalfa")));
	TestTrue(TEXT("customer (neutral) banner is emitted"), Neutral.Inl.Contains(TEXT("belongs to your project")));
	TestFalse(TEXT("customer output carries NO seller copyright"), Neutral.Inl.Contains(TEXT("All Rights Reserved")));
	return true;
}

// ── CG-3: a mis-cased host tag is rejected, not silently mapped to uncompilable C++ ─────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2TagCaseTest, "Ruitk.Bugfix2.TagCase",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2TagCaseTest::RunTest(const FString&)
{
	using namespace Bugfix2Test;
	// Canonical casing still compiles.
	const FUetkxCompileOutput Ok =
		FUetkxCodegen::CompileSource(TEXT("component A {\n\treturn (<TextBlock Text=\"x\"/>);\n}\n"), TEXT("A"));
	TestTrue(TEXT("canonical <TextBlock> compiles"), Ok.bOk);
	// A mis-cased variant is a hard error (UETKX0105), not silently normalized to broken code.
	const FUetkxCompileOutput Bad =
		FUetkxCodegen::CompileSource(TEXT("component B {\n\treturn (<Textblock Text=\"x\"/>);\n}\n"), TEXT("B"));
	TestFalse(TEXT("<Textblock> (wrong case) does not compile"), Bad.bOk);
	TestTrue(TEXT("<Textblock> reports UETKX0105"), DiagsContain(Bad.Diags, TEXT("UETKX0105")));
	return true;
}

// ── SCAN-1: a `<<`/comparison operator in a param DEFAULT must not merge later params ───────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2ParamOpTest, "Ruitk.Bugfix2.ParamOperator",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2ParamOpTest::RunTest(const FString&)
{
	const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(
		TEXT("component Foo(Mask: int32 = 1 << 3, Name: FText) {\n\treturn (<TextBlock Text=\"x\"/>);\n}\n"),
		TEXT("Foo"));
	if (!TestEqual(TEXT("one component scanned"), Scan.Components.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("both params survive the shift operator in the default"), Scan.Components[0].Params.Num(), 2);
	if (Scan.Components[0].Params.Num() == 2)
	{
		TestEqual(TEXT("param 0 is Mask"), Scan.Components[0].Params[0].Name, FString(TEXT("Mask")));
		TestEqual(TEXT("param 1 is Name"), Scan.Components[0].Params[1].Name, FString(TEXT("Name")));
	}
	return true;
}

// ── SCAN-2: a comment inside the import brace list must not drop the whole import ───────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2ImportCommentTest, "Ruitk.Bugfix2.ImportComment",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2ImportCommentTest::RunTest(const FString&)
{
	const FUetkxFileScanResult Scan =
		FUetkxFileScan::Scan(TEXT("import { A, /* keep */ B } from \"./x\"\n\ncomponent Foo {\n\treturn "
								  "(<TextBlock Text=\"x\"/>);\n}\n"),
							 TEXT("Foo"));
	if (!TestEqual(TEXT("one import scanned"), Scan.Imports.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("both names parsed across the comment"), Scan.Imports[0].Names.Num(), 2);
	return true;
}

// ── SCAN-3: a hook `-> Ret` with no body reports UETKX2202, not swallow the next declaration ────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2HookNoBodyTest, "Ruitk.Bugfix2.HookNoBody",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2HookNoBodyTest::RunTest(const FString&)
{
	using namespace Bugfix2Test;
	const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(
		TEXT("hook UseX() -> int32\n\ncomponent Foo {\n\treturn (<TextBlock Text=\"x\"/>);\n}\n"), TEXT("Foo"));
	TestTrue(TEXT("the missing hook body is reported (UETKX2202), not swallowing `component Foo`"),
			 DiagsContain(Scan.Diags, TEXT("UETKX2202")));
	// If a hook was captured, its return type must be bounded (not the swallowed next declaration).
	for (const FUetkxHookDecl& H : Scan.Hooks)
	{
		TestFalse(TEXT("hook return/body did not swallow the component"), H.Ret.Contains(TEXT("component")));
	}
	return true;
}

// ── STYLE-1: `$token` refs resolve against the active theme (bare AND `$`-declared forms) ────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2ThemeTokenTest, "Ruitk.Bugfix2.ThemeToken",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2ThemeTokenTest::RunTest(const FString&)
{
	Ruitk::Slate::LoadStylesheet(TEXT("@theme rui_b2 { accent: #4f8cff; $glow: #00ffcc; }"));
	Ruitk::Slate::SetActiveTheme(FName(TEXT("rui_b2")));

	TSharedRef<FRuitkStyleDict> Bare = MakeShared<FRuitkStyleDict>();
	Bare->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(FString(TEXT("$accent"))));
	const TSharedPtr<FRuitkStyleDict> EffBare = Ruitk::Slate::BuildEffectiveStyle({}, Bare);
	if (TestTrue(TEXT("bare token style resolves"), EffBare.IsValid()))
	{
		const FRuitkValue* V = EffBare->Find(FName(TEXT("ColorAndOpacity")));
		TestTrue(TEXT("$accent (bare decl) -> Color"), V && V->Kind == FRuitkValue::EKind::Color);
	}

	// STYLE-1: a `$`-prefixed DECLARATION is also accepted (stored bare) so the ref resolves.
	TSharedRef<FRuitkStyleDict> Dollar = MakeShared<FRuitkStyleDict>();
	Dollar->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(FString(TEXT("$glow"))));
	const TSharedPtr<FRuitkStyleDict> EffDollar = Ruitk::Slate::BuildEffectiveStyle({}, Dollar);
	if (TestTrue(TEXT("$-declared token style resolves"), EffDollar.IsValid()))
	{
		const FRuitkValue* V = EffDollar->Find(FName(TEXT("ColorAndOpacity")));
		TestTrue(TEXT("$glow ($-decl) -> Color"), V && V->Kind == FRuitkValue::EKind::Color);
	}
	Ruitk::Slate::SetActiveTheme(NAME_None);
	return true;
}

// ── STYLE-2: `"x, y"` (a space after the comma) parses as a Vector2, not a Name ─────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2Vector2Test, "Ruitk.Bugfix2.Vector2Space",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2Vector2Test::RunTest(const FString&)
{
	Ruitk::Slate::LoadStylesheet(TEXT(".rui_b2_vec { DesiredSizeOverride: 12, 34; }"));
	const FRuitkStyleDict* Cls = Ruitk::Slate::FindStyleClass(FName(TEXT("rui_b2_vec")));
	if (!TestNotNull(TEXT("class registered"), Cls))
	{
		return false;
	}
	const FRuitkValue* V = Cls->Find(FName(TEXT("DesiredSizeOverride")));
	if (TestTrue(TEXT("value present"), V != nullptr))
	{
		TestTrue(TEXT("space-after-comma parses as Vector2"), V->Kind == FRuitkValue::EKind::Vector2);
	}
	return true;
}

// ── IW-1: NumericEntryBox clamps to [Min, Max] (AllowSpin(false) never clamped in the engine) ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2NumericClampTest, "Ruitk.Bugfix2.NumericClamp",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2NumericClampTest::RunTest(const FString&)
{
	TSharedRef<SRuitkNumericEntryBox> Num =
		SNew(SRuitkNumericEntryBox).MinValue(TOptional<float>(0.0f)).MaxValue(TOptional<float>(100.0f));
	Num->SetValue(999.0f);
	TestEqual(TEXT("clamped to Max"), Num->GetValue(), 100.0f);
	Num->SetValue(-50.0f);
	TestEqual(TEXT("clamped to Min"), Num->GetValue(), 0.0f);
	Num->SetValue(42.0f);
	TestEqual(TEXT("in-range value untouched"), Num->GetValue(), 42.0f);
	return true;
}

// ── RECON-2: a POP (back) navigation honors an active UseBlocker ────────────────────────────────
namespace Bugfix2Test
{
	static TFunction<void(const FString&, bool)> GNav;
	static TFunction<void(int32)> GGo;
	static FString GLoc;
	static bool GBlockerFired = false;
	static TRuitkSetter<bool> GSetBlock;

	static FRuitkNodeArray BlockerProbe(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		auto [Block, SetBlock] = Ctx.UseState<bool>(false);
		GSetBlock = SetBlock;
		GNav = UseNavigate(Ctx);
		GGo = UseGo(Ctx);
		GLoc = UseLocation(Ctx).Pathname;
		UseBlocker(Ctx, Block, [](const FString&) { GBlockerFired = true; });
		return {Ruitk::TextBlock(GLoc)};
	}
	RUITK_COMPONENT(BlockerProbe)
} // namespace Bugfix2Test

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2BlockerPopTest, "Ruitk.Bugfix2.BlockerPop",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2BlockerPopTest::RunTest(const FString&)
{
	using namespace Bugfix2Test;
	GBlockerFired = false;
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::Router({Ruitk::FC(&BlockerProbe)}, TEXT("/")));
	Root->FlushSync();
	TestEqual(TEXT("starts at /"), GLoc, FString(TEXT("/")));

	GNav(TEXT("/b"), false); // push /b
	Root->FlushSync();
	TestEqual(TEXT("pushed to /b"), GLoc, FString(TEXT("/b")));

	GSetBlock(true); // activate the blocker
	Root->FlushSync();

	GGo(-1); // attempt back (POP)
	Root->FlushSync();
	TestTrue(TEXT("the blocker fired on the POP"), GBlockerFired);
	TestEqual(TEXT("back was blocked — still at /b"), GLoc, FString(TEXT("/b")));

	Root->Unmount();
	return true;
}

// ── DRV-2 / IMPORT-1: an unresolved import records the WOULD-BE label (reconstructable key) ──────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfix2WouldBeLabelTest, "Ruitk.Bugfix2.WouldBeLabel",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfix2WouldBeLabelTest::RunTest(const FString&)
{
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), {}, /*bFixtureMode*/ true);
	const FString Label = Resolver.WouldBeLabel(TEXT("./NoSuchFile"), TEXT("Sub/Importer.uetkx"));
	TestFalse(TEXT("would-be label is non-empty for a relative specifier"), Label.IsEmpty());
	TestTrue(TEXT("would-be label targets a .uetkx path"), Label.EndsWith(TEXT(".uetkx")));
	TestTrue(TEXT("but the missing file still fails to Resolve"),
			 Resolver.Resolve(TEXT("./NoSuchFile"), TEXT("Sub/Importer.uetkx")).IsEmpty());
	// A forbidden/anchorless specifier has no would-be label.
	TestTrue(TEXT("forbidden specifier -> empty would-be label"),
			 Resolver.WouldBeLabel(TEXT("res://x"), TEXT("Sub/Importer.uetkx")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
