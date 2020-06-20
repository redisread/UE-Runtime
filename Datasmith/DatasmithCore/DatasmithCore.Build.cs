// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithCore : ModuleRules
	{
		public DatasmithCore(ReadOnlyTargetRules Target)
            : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"RawMesh",
					"MeshDescription",
					"StaticMeshDescription"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Json",
					"XmlParser",
				}
			);
        }
	}
}