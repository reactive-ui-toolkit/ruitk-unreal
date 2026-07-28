// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Router.Match — the pure matching/parsing primitives (patterns, params, splats,
// search, resolve). Ruitk.Router.Spine — the navigation flow end to end: <Router> history,
// <Routes> matching + nested outlets, UseParams/UseSearchParams, back stack, and UseBlocker.

#include "Misc/AutomationTest.h"
#include "RuitkCoreElements.h"
#include "RuitkMockHost.h"
#include "RuitkNode.h"
#include "RuitkRouter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_ROUTER_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ── Ruitk.Router.Match ────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkRouterMatchTest, "Ruitk.Router.Match", RUITK_ROUTER_FLAGS)
bool FRuitkRouterMatchTest::RunTest(const FString&)
{
	// static + params + splats
	{
		FRuitkPathMatch M = Ruitk::MatchPath(TEXT("/users/:id"), TEXT("/users/42"));
		TestTrue(TEXT("param route matches"), M.bMatched);
		TestEqual(TEXT("param captured"), M.Params.FindRef(TEXT("id")), FString(TEXT("42")));
	}
	TestFalse(TEXT("leaf pattern rejects leftover path"), Ruitk::MatchPath(TEXT("/users"), TEXT("/users/42")).bMatched);
	TestTrue(TEXT("prefix (layout) accepts leftover"),
			 Ruitk::MatchPath(TEXT("/users"), TEXT("/users/42"), false).bMatched);
	{
		FRuitkPathMatch M = Ruitk::MatchPath(TEXT("/files/*"), TEXT("/files/a/b/c"));
		TestTrue(TEXT("splat matches"), M.bMatched);
		TestEqual(TEXT("splat captured"), M.Params.FindRef(TEXT("*")), FString(TEXT("a/b/c")));
	}
	TestTrue(TEXT("root matches root"), Ruitk::MatchPath(TEXT("/"), TEXT("/")).bMatched);
	TestFalse(TEXT("literal mismatch"), Ruitk::MatchPath(TEXT("/a"), TEXT("/b")).bMatched);

	// parse / build search
	{
		const TMap<FString, FString> P = Ruitk::ParseSearch(TEXT("?q=hi&page=2"));
		TestEqual(TEXT("search q"), P.FindRef(TEXT("q")), FString(TEXT("hi")));
		TestEqual(TEXT("search page"), P.FindRef(TEXT("page")), FString(TEXT("2")));
		TMap<FString, FString> Build;
		Build.Add(TEXT("b"), TEXT("2"));
		Build.Add(TEXT("a"), TEXT("1"));
		TestEqual(TEXT("build search is key-sorted"), Ruitk::BuildSearch(Build), FString(TEXT("?a=1&b=2")));
	}

	// parse location
	{
		const FRuitkLocation L = Ruitk::ParseLocation(TEXT("/shop/items?sort=price#top"));
		TestEqual(TEXT("pathname"), L.Pathname, FString(TEXT("/shop/items")));
		TestEqual(TEXT("search"), L.Search, FString(TEXT("?sort=price")));
		TestEqual(TEXT("hash"), L.Hash, FString(TEXT("#top")));
	}

	// resolve
	TestEqual(TEXT("absolute resolve"), Ruitk::ResolvePath(TEXT("/a/b"), TEXT("/x/y")), FString(TEXT("/a/b")));
	// React-Router treats the current pathname as a directory context: a bare relative segment
	// appends, and `..` climbs.
	TestEqual(TEXT("relative resolve appends"), Ruitk::ResolvePath(TEXT("edit"), TEXT("/users/42")),
			  FString(TEXT("/users/42/edit")));
	TestEqual(TEXT("dot-dot resolve climbs"), Ruitk::ResolvePath(TEXT("../list"), TEXT("/users/42")),
			  FString(TEXT("/users/list")));
	return true;
}

// ── Ruitk.Router.Spine ────────────────────────────────────────────────────────────────

namespace RouterTest
{
	static TFunction<void(const FString&, bool)> GNavigate;
	static TFunction<void()> GBack;
	static bool GCanGoBack = false;
	static TFunction<void(TMap<FString, FString>)> GSetSearch;
	static TMap<FString, FString> GSearch;
	static bool GBlock = false;
	static int32 GBlockedCount = 0;

	static FRuitkNodeArray UserScreen(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const TMap<FString, FString>& Params = UseParams(Ctx);
		return {Ruitk::TextBlock(TEXT("USER:") + Params.FindRef(TEXT("id")))};
	}
	RUITK_COMPONENT(UserScreen)

	static FRuitkNodeArray DashLayout(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		return {Ruitk::TextBlock(TEXT("LAYOUT")), UseOutlet(Ctx)};
	}
	RUITK_COMPONENT(DashLayout)

	// Captures navigation surfaces + registers a blocker; renders the declarative Routes.
	static FRuitkNodeArray Shell(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		GNavigate = UseNavigate(Ctx);
		TTuple<bool, TFunction<void()>> Back = UseBackStack(Ctx);
		GCanGoBack = Back.Key;
		GBack = Back.Value;
		TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>> SP = UseSearchParams(Ctx);
		GSearch = SP.Key;
		GSetSearch = SP.Value;
		UseBlocker(Ctx, GBlock, [](const FString&) { ++GBlockedCount; });

		TArray<Ruitk::FRuitkRoute> RouteList;
		RouteList.Add({TEXT("/home"), Ruitk::TextBlock(TEXT("HOME"))});
		RouteList.Add({TEXT("/users/:id"), Ruitk::FC(&UserScreen)});
		Ruitk::FRuitkRoute Dash;
		Dash.Path = TEXT("/dash");
		Dash.Element = Ruitk::FC(&DashLayout);
		Dash.Children.Add({FString(), Ruitk::TextBlock(TEXT("DASH-INDEX")), /*bIndex*/ true, {}});
		Dash.Children.Add({TEXT("settings"), Ruitk::TextBlock(TEXT("DASH-SETTINGS"))});
		RouteList.Add(Dash);

		return {Ruitk::Routes(MoveTemp(RouteList))};
	}
	RUITK_COMPONENT(Shell)

	static FRuitkNodeArray App(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		return {Ruitk::Router({Ruitk::FC(&Shell)}, TEXT("/home"))};
	}
	RUITK_COMPONENT(App)

	static void CollectText(FMockNode* Node, TArray<FString>& Out)
	{
		const FString T = Node->TextOf();
		if (!T.IsEmpty())
		{
			Out.Add(T);
		}
		for (const TSharedPtr<FMockNode>& C : Node->Children)
		{
			if (C.IsValid() && !C->bReleased)
			{
				CollectText(C.Get(), Out);
			}
		}
	}
} // namespace RouterTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkRouterSpineTest, "Ruitk.Router.Spine", RUITK_ROUTER_FLAGS)
bool FRuitkRouterSpineTest::RunTest(const FString&)
{
	using namespace RouterTest;
	GBlock = false;
	GBlockedCount = 0;

	FRuitkTestHarness H;
	auto Texts = [&H]()
	{
		TArray<FString> Out;
		CollectText(&H.Host.Root.Get(), Out);
		return Out;
	};
	auto Nav = [&H](const FString& To, bool bReplace = false)
	{
		GNavigate(To, bReplace);
		H.Reconciler->FlushSync();
	};

	H.Mount(Ruitk::FC(&App));

	TestTrue(TEXT("initial route /home"), Texts().Contains(FString(TEXT("HOME"))));

	Nav(TEXT("/users/42"));
	TestTrue(TEXT("param screen renders with param"), Texts().Contains(FString(TEXT("USER:42"))));
	TestFalse(TEXT("home gone"), Texts().Contains(FString(TEXT("HOME"))));

	// nested layout + index outlet
	Nav(TEXT("/dash"));
	{
		const TArray<FString> T = Texts();
		TestTrue(TEXT("layout renders"), T.Contains(FString(TEXT("LAYOUT"))));
		TestTrue(TEXT("index outlet renders"), T.Contains(FString(TEXT("DASH-INDEX"))));
	}
	Nav(TEXT("/dash/settings"));
	{
		const TArray<FString> T = Texts();
		TestTrue(TEXT("layout still renders"), T.Contains(FString(TEXT("LAYOUT"))));
		TestTrue(TEXT("settings outlet renders"), T.Contains(FString(TEXT("DASH-SETTINGS"))));
		TestFalse(TEXT("index gone"), T.Contains(FString(TEXT("DASH-INDEX"))));
	}

	// back stack: /home -> /users/42 -> /dash -> /dash/settings; go back once -> /dash
	TestTrue(TEXT("can go back"), GCanGoBack);
	GBack();
	H.Reconciler->FlushSync();
	TestTrue(TEXT("back to /dash index"), Texts().Contains(FString(TEXT("DASH-INDEX"))));

	// search params: navigate with a query, read it, then set it
	Nav(TEXT("/home?q=hi"));
	TestEqual(TEXT("search param read"), GSearch.FindRef(TEXT("q")), FString(TEXT("hi")));
	GSetSearch(
		[]
		{
			TMap<FString, FString> M;
			M.Add(TEXT("q"), TEXT("bye"));
			return M;
		}());
	H.Reconciler->FlushSync();
	TestEqual(TEXT("search param updated"), GSearch.FindRef(TEXT("q")), FString(TEXT("bye")));

	// blocker: while blocking, a navigate is intercepted (element does not change)
	GBlock = true;
	H.Reconciler->HmrRefreshAll(); // re-render so UseBlocker sees bBlock=true
	H.Reconciler->FlushSync();
	const int32 BlockedBefore = GBlockedCount;
	Nav(TEXT("/users/7"));
	TestEqual(TEXT("blocked navigation fired the blocker"), GBlockedCount, BlockedBefore + 1);
	TestFalse(TEXT("blocked navigation did not change route"), Texts().Contains(FString(TEXT("USER:7"))));

	GBlock = false;
	GNavigate = nullptr;
	GBack = nullptr;
	GSetSearch = nullptr;
	return true;
}

// ── Ruitk.Router.Hooks — the previously-untested hook set (audit §13) ─────────────────
// UseInRouterContext, UseLocation, UsePathname, UseSearch, UseNavigationType, UseMatch,
// UseIsActive, UseResolvedPath, UseHref, UseGo, and the Link component.

namespace RouterHooksTest
{
	static bool GInRouter = false;
	static FString GLocPath, GPathname, GSearchStr, GMatchId, GResolved, GHref;
	static bool GActivePrefix = false, GActiveExact = false;
	static int32 GNavType = -1;
	static TFunction<void(const FString&, bool)> GNavigate;
	static TFunction<void(int32)> GGo;

	static FRuitkNodeArray Probe(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		GInRouter = UseInRouterContext(Ctx);
		GLocPath = UseLocation(Ctx).Pathname;
		GPathname = UsePathname(Ctx);
		GSearchStr = UseSearch(Ctx);
		GNavType = static_cast<int32>(UseNavigationType(Ctx));
		GMatchId = UseMatch(Ctx, TEXT("/users/:id")).Params.FindRef(TEXT("id"));
		GActivePrefix = UseIsActive(Ctx, TEXT("/users"));
		GActiveExact = UseIsActive(Ctx, TEXT("/users"), /*bEnd*/ true);
		GResolved = UseResolvedPath(Ctx, TEXT("edit"));
		GHref = UseHref(Ctx, TEXT("../7"));
		return {Ruitk::Link(TEXT("/home"), {Ruitk::TextBlock(TEXT("LINK-CHILD"))})};
	}
	RUITK_COMPONENT(Probe)

	static FRuitkNodeArray Shell(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		GNavigate = UseNavigate(Ctx);
		GGo = UseGo(Ctx);
		TArray<Ruitk::FRuitkRoute> RouteList;
		RouteList.Add({TEXT("/home"), Ruitk::TextBlock(TEXT("HOOKS-HOME"))});
		RouteList.Add({TEXT("/users/:id"), Ruitk::FC(&Probe)});
		return {Ruitk::Routes(MoveTemp(RouteList))};
	}
	RUITK_COMPONENT(Shell)

	static FRuitkNodeArray App(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		return {Ruitk::Router({Ruitk::FC(&Shell)}, TEXT("/users/42?tab=stats"))};
	}
	RUITK_COMPONENT(App)

	// A router-less probe: UseInRouterContext must say false outside any <Router>.
	static bool GOutsideInRouter = true;
	static FRuitkNodeArray Outside(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		GOutsideInRouter = UseInRouterContext(Ctx);
		return {Ruitk::TextBlock(TEXT("OUTSIDE"))};
	}
	RUITK_COMPONENT(Outside)
} // namespace RouterHooksTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkRouterHooksTest, "Ruitk.Router.Hooks", RUITK_ROUTER_FLAGS)
bool FRuitkRouterHooksTest::RunTest(const FString&)
{
	using namespace RouterHooksTest;

	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&App));

	auto Texts = [&H]()
	{
		TArray<FString> Out;
		RouterTest::CollectText(&H.Host.Root.Get(), Out);
		return Out;
	};

	// Location family, on the initial /users/42?tab=stats
	TestTrue(TEXT("in router context"), GInRouter);
	TestEqual(TEXT("UseLocation pathname"), GLocPath, FString(TEXT("/users/42")));
	TestEqual(TEXT("UsePathname"), GPathname, FString(TEXT("/users/42")));
	TestEqual(TEXT("UseSearch"), GSearchStr, FString(TEXT("?tab=stats")));
	TestEqual(TEXT("UseMatch param"), GMatchId, FString(TEXT("42")));
	TestTrue(TEXT("UseIsActive prefix matches"), GActivePrefix);
	TestFalse(TEXT("UseIsActive bEnd rejects deeper path"), GActiveExact);
	TestEqual(TEXT("UseResolvedPath appends"), GResolved, FString(TEXT("/users/42/edit")));
	TestEqual(TEXT("UseHref climbs"), GHref, FString(TEXT("/users/7")));
	TestTrue(TEXT("Link renders its children"), Texts().Contains(FString(TEXT("LINK-CHILD"))));

	// Navigation type distinguishes push from replace (values are opaque; difference is the API).
	GNavigate(TEXT("/users/7"), /*bReplace*/ false);
	H.Reconciler->FlushSync();
	const int32 PushType = GNavType;
	TestEqual(TEXT("navigated (push)"), GPathname, FString(TEXT("/users/7")));
	GNavigate(TEXT("/users/8"), /*bReplace*/ true);
	H.Reconciler->FlushSync();
	TestEqual(TEXT("navigated (replace)"), GPathname, FString(TEXT("/users/8")));
	TestNotEqual(TEXT("push and replace report different navigation types"), PushType, GNavType);

	// UseGo: delta -1 walks history (the replace collapsed /users/7 -> /users/8).
	GGo(-1);
	H.Reconciler->FlushSync();
	TestEqual(TEXT("UseGo(-1) went back"), GPathname, FString(TEXT("/users/42")));

	GNavigate = nullptr;
	GGo = nullptr;

	// Outside any router: UseInRouterContext reports false.
	{
		FRuitkTestHarness H2;
		H2.Mount(Ruitk::FC(&Outside));
		TestFalse(TEXT("UseInRouterContext false outside a Router"), GOutsideInRouter);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
