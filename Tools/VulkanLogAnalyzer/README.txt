There is the console command 
r_logGPUStats N
where N - number of frames to log

it puts into output log information about GPU and CPU threads in format like:

<Info From Device> (GPU/CPU) - Begin Frame at 0.375855. Frame num=2951
<Info From Device> (GPU/CPU) - frame 2950, commit 0.328881, begin: 0.330713, end: 0.335301

You should extract them from the log with your preffered tool and remove <Info From Device> at the start of each line.
Then you should remove duplicated lines with remove_duplicates.py:

python remove_duplicates.py input.txt output.txt


You should install python package matplotlib:

pip install matplotlib

Now you can get .png file with the visual representation of how GPU and CPU threads work

python vulkan_timeline_plot.py output.txt

As a result you will get "vulkan_timeline.png"
