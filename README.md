AzurenderPlus Project Description:
------------
<p>AzurenderPlus is for final project of 15-869: Visual Computing System at CMU.</p>
<p>The goal is to build an out-of-core sort-based deferred ray tracer.</p>
Project proposal page: http://photonmap.blogspot.com/2014/11/15-869-final-project-proposal-cpu-based.html


<h3>Change Logs:</h3>

11/5/2014
Removed path tracing code and photon mapping code from original Azurender code base for simplicity. This is the baseline.

11/6/2014
Update baseline for the final project. Added azRayPacket class for further working.

12/2/2014
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


