Introduction
    
The scripts under viasat folder are utility scripts to run the UniPD NS3 deliveries.



docker_build.sh

This script builds the Ubuntu docker container with dependencies preinstalled.

From viasat/ folder run the following command to build the container
    
    ./docker_build.sh

docker_run.sh

This script will start the Ubuntu docker container and launch bash.

    ./docker_run.sh

All the commands specified in top level [Readme](../Readme.md) can be run in this container's bash.

ns_log_enable.sh

This script launches one instance of a simulation with targeted debug statements.

    source ns_log_enable.sh

For further information on enabling specific logging please refer the [documentation](https://www.nsnam.org/docs/release/3.33/manual/html/logging.html)


aws_init.sh
 
    This script installs all the necessary dependencies in the Ubuntu VM

aws_ns3_start.sh

After uploading the UniPD deliverables (entire git code) as inmarsat-iab-releases.zip into the VM, starting this script will launch the simulation.

Refer https://inmarsat.atlassian.net/wiki/spaces/ON/pages/4166254635/How+to+run+it+in+AWS+EC2

Alternatively the simulation can be started with the container.