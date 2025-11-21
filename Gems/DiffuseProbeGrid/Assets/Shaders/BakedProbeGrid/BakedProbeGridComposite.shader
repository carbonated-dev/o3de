{
    "Source" : "BakedProbeGridComposite.azsl",

    "DrawList" : "bakedProbeComposite",
	
    "DepthStencilState" :
    {
        "Depth" :
        {
            "Enable" : false
        }
    },	

    "ProgramSettings":
    {
        "EntryPoints":
        [
            { "name": "MainVS", "type": "Vertex" },
            { "name": "MainPS", "type": "Fragment" }
        ]
    },

    "Supervariants":
    [
        {
            "Name": "Main"
        }
    ]
}
