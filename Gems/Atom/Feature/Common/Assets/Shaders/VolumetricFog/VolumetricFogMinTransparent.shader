{
    "Source" : "VolumetricFogMinMaxTransparent.azsl",

    "RasterState": { "CullMode": "None" },

    "DepthStencilState" : { 
        "Depth" : { "Enable" : true, "CompareFunc" : "GreaterEqual" }
    },

    "ProgramSettings" : 
    {
        "EntryPoints":
        [
            {
                "name": "MainVS",
                "type" : "Vertex"
            }
        ] 
    },

    "DrawList" : "depthTransparentMin",
    "Supervariants":
    [
        {
            "Name": "",
            "AddBuildArguments": {
                "debug": true
            }
        }
    ]
}
