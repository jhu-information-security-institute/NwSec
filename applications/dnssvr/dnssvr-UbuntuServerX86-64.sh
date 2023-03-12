# Script to clone specific files for creating a dns server environment within a Docker container
# running a minimal Ubuntu server image

# Create a directory, so Git doesn't get messy, and enter it
mkdir dnssvr && cd dnssvr

# Start a Git repository
git init

# Squelch annoying message
git config pull.rebase false  # merge (the default strategy)

# Track repository, do not enter subdirectory
git remote add -f origin https://github.com/jhu-information-security-institute/NwSec.git

# Enable the tree check feature
git config core.sparseCheckout true

# Create a file in the path: .git/info/sparse-checkout
# That is inside the hidden .git directory that was created
# by running the command: git init
# And inside it enter the name of the specific files (or subdirectory) you only want to clone
echo 'applications/dnssvr/UbuntuServerX86-64/.project' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/Dockerfile' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_bind_named.conf.local' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_bind_named.conf.options' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_bind_zones_db.25.168.192' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_bind_zones_db.netsec-docker.isi.jhu.edu' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_default_named' >> .git/info/sparse-checkout
echo 'applications/dnssvr/UbuntuServerX86-64/etc_resolv.conf' >> .git/info/sparse-checkout

## Download with pull, not clone
git pull origin master

echo 'cd into dnssvr/applications/dnssvr/UbuntuServerX86-64 and view details in Dockerfile for building, running, and attaching to the container'

# References:
#   https://terminalroot.com/how-to-clone-only-a-subdirectory-with-git-or-svn

