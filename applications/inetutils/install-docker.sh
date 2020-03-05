#!/usr/bin/env bash
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install apt-transport-https ca-certificates curl gnupg-agent software-properties-common -y
cd /tmp
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository "deb [arch=arm64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io -y
sudo docker run hello-world

echo 'Finished installing Docker. Cool!'
