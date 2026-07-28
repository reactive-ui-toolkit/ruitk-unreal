// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-001 — the family router, ported engine-blind (RuitkCore, no UObject). A React-Router-
// shaped, in-memory router: a <Router> provides an in-memory history; <Routes>/Ruitk::Routes match
// the current location against a pattern tree (`/users/:id`, `/files/*`); the hooks read the
// match + drive navigation. Everything is pure logic over context — portable to the Unity/Godot
// siblings as-is.
//
// Path patterns: `/a/b` literal, `:name` a named param (one segment), `*` a trailing splat
// (captured as the "*" param). Matching is case-sensitive, slash-normalized, and (for nested
// routes) prefix-aware.

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"

class FRuitkContext;

/** How the current location was reached (React-Router's NavigationType). */
enum class ERuitkNavigationType : uint8
{
	Push,
	Replace,
	Pop
};

/** A parsed location: pathname + search (`?a=b`) + hash (`#x`). No origin — routing is in-memory. */
struct RUITKCORE_API FRuitkLocation
{
	FString Pathname = TEXT("/");
	FString Search; // includes the leading '?', or empty
	FString Hash;	// includes the leading '#', or empty
	FString Key;	// a per-entry key (history identity)

	bool operator==(const FRuitkLocation& Other) const
	{
		return Pathname == Other.Pathname && Search == Other.Search && Hash == Other.Hash;
	}
	bool operator!=(const FRuitkLocation& Other) const { return !(*this == Other); }

	/** Reassemble `pathname?search#hash`. */
	FString ToHref() const { return Pathname + Search + Hash; }
};

/** The result of matching a pattern against a pathname. */
struct RUITKCORE_API FRuitkPathMatch
{
	bool bMatched = false;
	TMap<FString, FString> Params;
	FString Pathname;	  // the portion that matched
	FString PathnameBase; // the base consumed before this match (nested routes)

	explicit operator bool() const { return bMatched; }
};

namespace Ruitk
{
	// ── pure helpers (the router_match suite exercises these directly) ─────────────────────

	/** Match `Pattern` against `Pathname`. `bEnd` = the pattern must consume the WHOLE pathname
	 *  (a leaf route); false allows a prefix match (a layout route with nested children). */
	RUITKCORE_API FRuitkPathMatch MatchPath(const FString& Pattern, const FString& Pathname, bool bEnd = true);

	/** Parse `pathname?search#hash` into a location. */
	RUITKCORE_API FRuitkLocation ParseLocation(const FString& Href);

	/** Parse a `?a=b&c=d` search string into a key→value map (first value wins). */
	RUITKCORE_API TMap<FString, FString> ParseSearch(const FString& Search);

	/** Build a `?a=b&c=d` search string from a map (stable key order). */
	RUITKCORE_API FString BuildSearch(const TMap<FString, FString>& Params);

	/** Resolve `To` against `From` (absolute `To` wins; relative joins onto From's directory). */
	RUITKCORE_API FString ResolvePath(const FString& To, const FString& From);

	// ── the <Router> provider + declarative <Routes> + <Link> ──────────────────────────────

	/** A single declarative route: a `Path` pattern → `Element`, with optional nested `Children`
	 *  (a layout route whose Element should render an <Outlet/> = UseOutlet()). An empty Path with
	 *  bIndex marks the index route (matches the parent exactly). */
	struct RUITKCORE_API FRuitkRoute
	{
		FString Path;
		FRuitkNode Element;
		bool bIndex = false;
		TArray<FRuitkRoute> Children;
	};

	/** The in-memory router boundary. `InitialPath` seeds the first location. */
	RUITKCORE_API FRuitkNode Router(TArray<FRuitkNode> Children, FString InitialPath = TEXT("/"),
									   FRuitkKey Key = FRuitkKey());

	/** Render the best match for the current location from `RouteList` (nesting via Outlet). */
	RUITKCORE_API FRuitkNode Routes(TArray<FRuitkRoute> RouteList, FRuitkKey Key = FRuitkKey());

	/** A navigation link: renders `Children` and navigates to `To` on click (needs a host
	 *  wrapper that forwards clicks — the Slate Button/Text `OnClicked`; this is the core node). */
	RUITKCORE_API FRuitkNode Link(FString To, TArray<FRuitkNode> Children, bool bReplace = false,
									 FRuitkKey Key = FRuitkKey());
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// The 17 family router hooks (free functions taking FRuitkContext& first — the family convention).
// ─────────────────────────────────────────────────────────────────────────────────────────

/** True when called inside a <Router>. */
RUITKCORE_API bool UseInRouterContext(FRuitkContext& Ctx);

/** The current location (pathname/search/hash). */
RUITKCORE_API const FRuitkLocation& UseLocation(FRuitkContext& Ctx);

/** Convenience: the current pathname. */
RUITKCORE_API FString UsePathname(FRuitkContext& Ctx);

/** Convenience: the current search string (with the leading '?'). */
RUITKCORE_API FString UseSearch(FRuitkContext& Ctx);

/** How the current location was reached. */
RUITKCORE_API ERuitkNavigationType UseNavigationType(FRuitkContext& Ctx);

/** Imperative navigation: To may be absolute or relative; bReplace swaps the top entry. */
RUITKCORE_API TFunction<void(const FString& To, bool bReplace)> UseNavigate(FRuitkContext& Ctx);

/** Relative history motion: Go(-1) = back, Go(1) = forward. */
RUITKCORE_API TFunction<void(int32 Delta)> UseGo(FRuitkContext& Ctx);

/** { bCanGoBack, GoBack() } — the common back-button surface. */
RUITKCORE_API TTuple<bool, TFunction<void()>> UseBackStack(FRuitkContext& Ctx);

/** The params captured by the nearest matched route. */
RUITKCORE_API const TMap<FString, FString>& UseParams(FRuitkContext& Ctx);

/** [ params, SetParams ] over the location's search string. */
RUITKCORE_API TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>>
UseSearchParams(FRuitkContext& Ctx);

/** Match `Pattern` against the current pathname (nullptr-ish via bMatched). */
RUITKCORE_API FRuitkPathMatch UseMatch(FRuitkContext& Ctx, const FString& Pattern);

/** True when `Pattern` matches the current pathname (NavLink active state). */
RUITKCORE_API bool UseIsActive(FRuitkContext& Ctx, const FString& Pattern, bool bEnd = false);

/** Resolve `To` against the current location's pathname. */
RUITKCORE_API FString UseResolvedPath(FRuitkContext& Ctx, const FString& To);

/** The href a link to `To` would point at (resolved + reassembled). */
RUITKCORE_API FString UseHref(FRuitkContext& Ctx, const FString& To);

/** The nested matched child element (rendered by a layout route's Element). */
RUITKCORE_API FRuitkNode UseOutlet(FRuitkContext& Ctx);

/** Declarative routing as a hook: render the best match for `RouteList`. */
RUITKCORE_API FRuitkNode UseRoutes(FRuitkContext& Ctx, const TArray<Ruitk::FRuitkRoute>& RouteList);

/** Register a navigation blocker: while `bBlock`, navigations are intercepted and `OnBlocked`
 *  (the attempted href) fires instead of committing. Returns nothing (fire-and-forget guard). */
RUITKCORE_API void UseBlocker(FRuitkContext& Ctx, bool bBlock,
								   TFunction<void(const FString& AttemptedHref)> OnBlocked);
