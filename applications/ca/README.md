# Runtime environment setup
## Prerequisites
1. webproxy container setup and running
1. Create a dns entry for virtual interface on infrastructure vm: ca.netsec-docker.isi.jhu.edu on 192.168.25.15

## Server (on VM)
1. Pull the smallstep image: $ docker pull smallstep/step-ca
1. If you have run this previously, delete any existing docker volumes created (should be named step)
1. Run the container: $ docker run --name ca -it -v step:/home/step -p 9000:9000 -e "DOCKER_STEPCA_INIT_NAME=Smallstep" -e "DOCKER_STEPCA_INIT_DNS_NAMES=localhost,ca.netsec-docker.isi.jhu.edu" -e "DOCKER_STEPCA_INIT_REMOTE_MANAGEMENT=true" -e "DOCKER_STEPCA_INIT_PASSWORD=student" smallstep/step-ca
1. Confirm the server is operational by viewing: https://ca.netsec-docker.isi.jhu.edu:9000/health
2. 
