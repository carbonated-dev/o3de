{
    "Source" : "SilhouetteJFAStep.azsl",
    "DepthStencilState" : {
        "Depth": 
        {
            "Enable": false,
            "CompareFunc" : "Always"
        }
    },
    "GlobalTargetBlendState": {
        "Enable": false
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
            "Name": "NoMSAA",
            "AddBuildArguments": {
                "azslc": ["--no-ms"]
            }
        }
    ]
}
