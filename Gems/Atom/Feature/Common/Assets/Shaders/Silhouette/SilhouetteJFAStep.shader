{
    "Source" : "SilhouetteJFAStep.azsl",
    "DepthStencilState" : {
        "Depth": 
        {
            "Enable": false,
            "CompareFunc" : "Always"
        },
        "Stencil" :
        {
            "Enable" : true,
            "ReadMask" : "0x20",
            "WriteMask" : "0x00",
            "FrontFace" :
            {
                "Func" : "Equal",
                "DepthFailOp" : "Keep",
                "FailOp" : "Keep",
                "PassOp" : "Keep"
            }
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
