# Adding a new slurm plugin
# ==========================

This scripts and folders patch slurm to add a new select plugin that
enables to execute any external allocator.

To intall:
* Run deploy script (will copy files to the VM holding slurm simulator)
* Patch configure.ac in slurm, to add new plugin
* Run ./autogen.sh to generate Makefile.in files
* Install simulator via ./install.pl script in the simulator folder

