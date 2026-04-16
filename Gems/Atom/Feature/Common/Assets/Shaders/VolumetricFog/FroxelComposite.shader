{
    "Source" : "FroxelComposite.azsl",
    "DepthStencilState" : {
        "Depth": 
        {
            "Enable": false,
            "CompareFunc" : "Always"
        }
    },
    "GlobalTargetBlendState": {
        "Enable": true,
        "BlendSource":      "One",
        "BlendDest":        "AlphaSource",
        "BlendOp":          "Add",
        "BlendAlphaSource": "Zero",
        "BlendAlphaDest":   "One",
        "BlendAlphaOp":     "Add"
    },
    "ProgramSettings": {
        "EntryPoints": [
        {
            "name": "MainVS",
            "type" : "Vertex"
        },
        {
            "name": "MainPS",
            "type" : "Fragment"
        }
        ]
    },
    "Supervariants":
    [
        {
            "Name": "",
            "AddBuildArguments": {
                "debug": true
            }
        },
        {
            "Name": "NoMSAA",
            "AddBuildArguments": {
                "azslc": ["--no-ms"]
            }
        }
    ]
}
