# Runtime environment setup
<img width="561" height="531" alt="https_ca_proxy_load_balancer drawio" src="https://github.com/user-attachments/assets/115631e1-d0d6-4dd7-a32d-a74df7cc64fc" />

## Prerequisites
1. webproxy container setup and running
1. Create a dns entry for virtual interface on infrastructure vm: ca.netsec-docker.isi.jhu.edu on 192.168.25.15

## Server (on infrastructure VM)
1. Pull the smallstep image on your infrastructure VM: $ docker pull smallstep/step-ca
1. If you have run this previously, delete any existing docker volumes created (should be named step)
1. Run the container: $ docker run --name ca -it -v step:/home/step -p 9000:9000 -e "DOCKER_STEPCA_INIT_NAME=Smallstep" -e "DOCKER_STEPCA_INIT_DNS_NAMES=localhost,ca.netsec-docker.isi.jhu.edu" -e "DOCKER_STEPCA_INIT_REMOTE_MANAGEMENT=true" -e "DOCKER_STEPCA_INIT_PASSWORD=student" smallstep/step-ca
1. Confirm the server is operational by viewing: https://ca.netsec-docker.isi.jhu.edu:9000/health
1. Query the root ca fingerprint from the smallstep container using: `$ docker run -v step:/home/step smallstep/step-ca step certificate fingerprint certs/root_ca.crt`
1. Copy the root certificate from the container into the VM and then scp it over to your kali VM to use later: `$ docker cp ca:/home/step/certs/root_ca.crt ~/.`
## Server (on target VM)
1. For server's container, install step client and bootstrap it to the smallstep server
    ```
    # apt-get install curl gpg ca-certificates
    # curl -fsSL https://packages.smallstep.com/keys/apt/repo-signing-key.gpg -o /etc/apt/keyrings/smallstep.asc
    # cat << EOF > /etc/apt/sources.list.d/smallstep.sources
    Types: deb
    URIs: https://packages.smallstep.com/stable/debian
    Suites: debs
    Components: main
    Signed-By: /etc/apt/keyrings/smallstep.asc
    EOF
    # apt-get update && apt-get -y install step-cli
    ```
1. Test the installation by running: `# step certificate inspect https://smallstep.com`
1. Configure the step client with the smallstep server by securely fetching the root_ca certificate: `# step ca bootstrap --ca-url https://ca.netsec-docker.isi.jhu.edu:9000 --fingerprint <CA fingerprint>`
1. Establish system-wide trust of CA: `# step certificate install /root/.step/certs/root_ca.crt`
1. Generate a private key for server and obtain a signed certificate (you will be prompted for ca server's administrative password, it was fixed in the ca's installation to 'student')
    * Replace <HOSTNAME> below with the server's hostname (e.g., proxy, email)
    * Replace <CERTNAME> below with a name corresponding to the host name (e.g., proxy_ca.crt, email_ca.crt)
    * Replace <KEYNAME> below with a name corresponding to the host name (e.g., proxy_ca.crt.key, email_ca.crt.key)
    ```
    $ mkdir -p /certs
    $ step ca certificate proxy.netsec-docker.isi.jhu.edu /certs/proxy_ca.crt /certs/proxy_ca.crt.key  --not-after=12m
    ```
## Client (on Kali VM) browser
1. Copy the root_ca.crt into the Kali VM
1. In client browser, add the CA by importing root_ca.crt
    1. Firefox: Settings -> Privacy & Security -> Certificates -> View Certificates
    1. click Import and select your root_ca.crt file (ensure you enable trust for identifying websites)
1. Go to: https://proxy.netsec-docker.isi.jhu.edu:4443/

## Client (on Kali VM) thunderbird
1. Copy the root_ca.crt into the attack container
1. In thunderbird, add the CA by importing root_ca.crt
    1. Thunderbird: Edit -> Settings, select Privacy and Security tab (on left), scroll down and click Manage Certificates button
    2. On Authorities, click Import and select your root_ca.crt file (ensure you enable trust for identify email users)

## Useful websites
* https://hub.docker.com/r/smallstep/step-ca
* https://smallstep.com/docs/step-ca/basic-certificate-authority-operations
* https://smallstep.com/docs/step-cli/installation
* https://www.haproxy.com/documentation/haproxy-configuration-tutorials/security/ssl-tls/client-side-encryption


