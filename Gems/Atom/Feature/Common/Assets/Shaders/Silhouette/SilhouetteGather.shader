{
    "Source" : "SilhouetteGather.azsl",
    "DepthStencilState" : {
        "Depth": 
        {
            "Enable": true, 
            "WriteMask" : "Zero",   // Avoid writing the depth
            "CompareFunc" : "Less"
        },
        "Stencil" :
        {
            "Enable" : true,
            "ReadMask" : "0x0",
            "WriteMask" : "0x20",
            "FrontFace" :
            {
                "Func" : "Always",
                "DepthFailOp" : "Keep",
                "FailOp" : "Keep",
                "PassOp" : "Replace"
            }
        }
    },
    "DrawList": "silhouette",
    "RasterState": { 
        "CullMode": "Back"
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
