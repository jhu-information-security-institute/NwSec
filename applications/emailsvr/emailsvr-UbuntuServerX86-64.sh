# Script to clone specific files for creating an email server environment within a Docker container
# running a minimal Ubuntu server image

# Create a directory, so Git doesn't get messy, and enter it
mkdir emailsvr && cd emailsvr

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
echo 'applications/emailsvr/UbuntuServerX86-64/.project' >> .git/info/sparse-checkout
echo 'applications/emailsvr/UbuntuServerX86-64/Dockerfile' >> .git/info/sparse-checkout
echo 'applications/emailsvr/UbuntuServerX86-64/dovecot' >> .git/info/sparse-checkout
echo 'applications/emailsvr/UbuntuServerX86-64/postfix' >> .git/info/sparse-checkout
echo 'applications/emailsvr/UbuntuServerX86-64/postfixadmin' >> .git/info/sparse-checkout

## Download with pull, not clone
git pull origin main

echo 'cd into emailsvr/applications/emailsvr/UbuntuServerX86-64 and view details in Dockerfile for building, running, and attaching to the container'

# References:
#   https://terminalroot.com/how-to-clone-only-a-subdirectory-with-git-or-svn

