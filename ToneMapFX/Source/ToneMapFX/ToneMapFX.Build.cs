// Licensed under the zlib License. See LICENSE file in the project root.

using UnrealBuildTool;

public class ToneMapFX : ModuleRules
{
	public ToneMapFX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"Renderer",
				"RHI",
				"Projects"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"DesktopPlatform"
			}
		);

		// Access to private/internal Renderer headers (FViewInfo, SceneRendering, etc.)
		string RendererBase = System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Renderer");
		string RendererPrivate = System.IO.Path.Combine(RendererBase, "Private");
		string RendererInternal = System.IO.Path.Combine(RendererBase, "Internal");
		PrivateIncludePaths.AddRange(
			new string[]
			{
				// Private + all subdirectories
				RendererPrivate,
				System.IO.Path.Combine(RendererPrivate, "CompositionLighting"),
				System.IO.Path.Combine(RendererPrivate, "Froxel"),
				System.IO.Path.Combine(RendererPrivate, "HairStrands"),
				System.IO.Path.Combine(RendererPrivate, "HeterogeneousVolumes"),
				System.IO.Path.Combine(RendererPrivate, "InstanceCulling"),
				System.IO.Path.Combine(RendererPrivate, "Lumen"),
				System.IO.Path.Combine(RendererPrivate, "MaterialCache"),
				System.IO.Path.Combine(RendererPrivate, "MegaLights"),
				System.IO.Path.Combine(RendererPrivate, "Nanite"),
				System.IO.Path.Combine(RendererPrivate, "OIT"),
				System.IO.Path.Combine(RendererPrivate, "PostProcess"),
				System.IO.Path.Combine(RendererPrivate, "RayTracing"),
				System.IO.Path.Combine(RendererPrivate, "Renderer"),
				System.IO.Path.Combine(RendererPrivate, "SceneCulling"),
				System.IO.Path.Combine(RendererPrivate, "Shadows"),
				System.IO.Path.Combine(RendererPrivate, "Skinning"),
				System.IO.Path.Combine(RendererPrivate, "SparseVolumeTexture"),
				System.IO.Path.Combine(RendererPrivate, "StateStream"),
				System.IO.Path.Combine(RendererPrivate, "StochasticLighting"),
				System.IO.Path.Combine(RendererPrivate, "Substrate"),
				System.IO.Path.Combine(RendererPrivate, "Substrate/Glint"),
				System.IO.Path.Combine(RendererPrivate, "Tests"),
				System.IO.Path.Combine(RendererPrivate, "VariableRateShading"),
				System.IO.Path.Combine(RendererPrivate, "VirtualShadowMaps"),
				System.IO.Path.Combine(RendererPrivate, "VT"),
				// Internal + subdirectories (TranslucentPassResource.h, etc.)
				RendererInternal,
				System.IO.Path.Combine(RendererInternal, "MaterialCache"),
				System.IO.Path.Combine(RendererInternal, "PostProcess"),
				System.IO.Path.Combine(RendererInternal, "VT"),
			}
		);
	}
}
