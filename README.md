AzurenderPlus Project Description:
------------
<p>AzurenderPlus is for final project of 15-869: Visual Computing System at CMU.</p>
<p>The goal is to build an out-of-core sort-based deferred ray tracer.</p>
Project proposal page: http://photonmap.blogspot.com/2014/11/15-869-final-project-proposal-cpu-based.html


<h3>Change Logs:</h3>

<strong>11/5/2014</strong>

Removed path tracing code and photon mapping code from original Azurender code base for simplicity. This is the baseline.

<strong>11/6/2014</strong>

Update baseline for the final project. Added azRayPacket class for further working.

<strong>12/2/2014</strong>

Added OPEN MPI Supports. OPEN MPI libs are required. 
For Mac OSX: Use Homebrew install openmpi
For Ubuntu:  Use apt-get

To run the sample code on main.cpp, in your BUILD directory, cd into Debug, do as following: 

<code>mpirun -np 4 ./raytracer scenes/dragon_only.scene</code>

Ideally, you should see:

<code>Hello world from processor CMU-853713.WV.CC.CMU.EDU, rank 0 out of 4 processors</code>

<code>Hello world from processor CMU-853713.WV.CC.CMU.EDU, rank 3 out of 4 processors</code>

<code>Hello world from processor CMU-853713.WV.CC.CMU.EDU, rank 1 out of 4 processors</code>

<code>Hello world from processor CMU-853713.WV.CC.CMU.EDU, rank 2 out of 4 processors</code>


<strong>12/3/2014</strong>

Got multiple machines on GHC running on independent instances simultaneously. To do this, change your .cshrc file, add the following lines for setting up environment variables

<code>setenv PATH "/usr/lib64/openmpi/bin:/usr/local/bin:${PATH}"</code>
<code>setenv LD_LIBRARY_PATH "/usr/lib64/openmpi/lib"</code>

Then go to your build directory where the bin file exists, run the ray tracer with following command:

<code>mpirun -np 2 --host 128.2.100.173,128.2.100.175 ./raytracer scenes/dragon_only.scene -r</code>

The two ip addresses are GHC40 and GHC42. <code>-r</code> flag would prevent machines from opening a window, they would instead run the ray tracer on specific scene, and generate two independent png files.


<strong>12/4/2014</strong>

Created stanford dragon scene for testing. Added MPI basic interface and integrated data structure for ray redistribution through Open MPI. Refactorting of the matrix, ray class for  distributed ray tracing. Finished the redistribution of MPI nodes bounding boxes.


<strong>12/5/2014 - 12/6/2014</strong>

Finished MPI stage for eye ray redistribution, shadow ray distribution and local independent shading. 


<strong>12/7/2014 - 12/8/2014</strong>

Finished node framebuffer merging, refactored original single framebuffer to framebuffer with zbuffer integrated. Now we have a basic rendered result of piled stanford dragon:

![alt tag](https://raw.githubusercontent.com/analysiser/AzurenderPlus/926acf2de96a1af6fbb6e76fcf38233932d6094d/resources/images/2014120800.png)
