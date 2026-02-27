# Runtime environment setup
## Prerequisites
1. webproxy container setup and running
1. Create a dns entry for virtual interface on infrastructure vm: ca.netsec-docker.isi.jhu.edu on 192.168.25.15

## Server (on infrastructure VM)
1. Pull the smallstep image on your infrastructure VM: $ docker pull smallstep/step-ca
1. If you have run this previously, delete any existing docker volumes created (should be named step)
1. Run the container: $ docker run --name ca -it -v step:/home/step -p 9000:9000 -e "DOCKER_STEPCA_INIT_NAME=Smallstep" -e "DOCKER_STEPCA_INIT_DNS_NAMES=localhost,ca.netsec-docker.isi.jhu.edu" -e "DOCKER_STEPCA_INIT_REMOTE_MANAGEMENT=true" -e "DOCKER_STEPCA_INIT_PASSWORD=student" smallstep/step-ca
1. Confirm the server is operational by viewing: https://ca.netsec-docker.isi.jhu.edu:9000/health
1. Query the root ca fingerprint from the smallstep container using: `$ docker run -v step:/home/step smallstep/step-ca step certificate fingerprint certs/root_ca.crt`
## Server (on target VM)
1. For haproxy container, install step client and bootstrap it to the smallstep server
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
1. Configure the step client with the smallstep server: `# step ca bootstrap --ca-url https://ca.netsec-docker.isi.jhu.edu:9000 --fingerprint <CA fingerprint>`
1. Establish system-wide trust of CA: `# step certificate install /root/.step/certs/root_ca.crt`
1. Generate a private key for haproxy and obtain a signed certificate (you will be prompted for ca server's administrative password, it was fixed in a command above to 'student')
    ```
    $ mkdir -p /certs
    $ step ca certificate proxy.netsec-docker.isi.jhu.edu /certs/proxy_ca.crt /certs/proxy_ca.crt.key  --not-after=12m
    ```
1. Update frontend section in the /etc/haproxy/haproxy.cfg
    ```
    bind 0.0.0.0:443 ssl crt /certs
    ```
1. Alternately, force server to use https by also adding to the file above in the frontend section
    ```
    http-request redirect scheme https unless { ssl_fc }
    ```
## Client (on Kali VM)
1. In client browser, add the CA by importing root_ca.crt
    1. Firefox: Settings > Privacy & Security > Certificates > View Certificates
    1. click Import and select your root_ca.crt file
1. Go to: https://proxy.netsec-docker.isi.jhu.edu:4443/

## Useful websites
* https://hub.docker.com/r/smallstep/step-ca
* https://smallstep.com/docs/step-ca/basic-certificate-authority-operations
* https://smallstep.com/docs/step-cli/installation
* https://www.haproxy.com/documentation/haproxy-configuration-tutorials/security/ssl-tls/client-side-encryption


