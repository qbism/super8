Effects comments from dcemulation.org board (thought this was interesting - qbism)

---------
FEATURE REUQEST cvar that makes second sky scroll layer additive if addmap exists

It's funny that you mentioned the additive sky blending, because I had done this for Fightoon (hard coded, no cvar) and reverted it for Makaqu 1.3. There's actually a handful of interesting blending effects that would work well in different situations, and it would be interesting to implement a way to select between different blend maps not only for the sky but also for the other parts of the renderer that uses blending effects (liquids, sprites, alias models, console background, etc.).

Personally I'd like to replace EF_ADDITIVE with a string field called .r_blendmap and a float field called .r_blendintensity, and EF_CELSHADING with a float field called .r_shading (.r_shading would also replace EF_FULLBRIGHT and the metallic shading whose EF_ name I don't remember right now) and another called .r_outline (which could probably be replaced with a .r_wireframe), but I don't see the point in making all these standards for an obscure engine such as Makaqu.

By the way, I just remembered that Makaqu supports multiple sky textures in the same map (normal Quake engines always uses the same sky texture on all sky surfaces). And Makaqu 1.3 can display skyboxes in maps without sky surfaces (the skyboxes replaces the void, noclip out of a map to see).

Well, enough rambling. Currently I'm kinda busy working in a Java dancing game, but when I get enough free time I'll work on Makaqu 1.4, which should be the last release.


