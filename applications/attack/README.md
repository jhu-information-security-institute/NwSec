# Application overview
* Denial of service attacks on Nginx using hping3
* Username guessing attacks (via RCPT TO) on Postfix using ismtp
* Email spoofing attacks on Postfix using SEToolkit
* Brute-force dictionary attacks on Dovecot using hydra
* Firefox web client
* Thunderbird email client

# Runtime environment setup
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/main/applications/attack/attack-KaliX86-64.sh
    $ chmod +x attack-KaliX86-64.sh
    $ ./attack-KaliX86-64.sh
    ```
1. Create a Kali linux container using the instructions from [here](https://github.com/jhu-information-security-institute/netsec-kalilinux-docker)
1. Run the steps in [Ansible-setup](https://github.com/jhu-information-security-institute/infrastructure/wiki/Ansible-setup)
    * Steps might require tailoring to your host
1. Run the playbook: `(ansible)$ ansible-playbook setup.yml -v -i inventory -u <USERNAME> --extra-vars "USERNAME=<USERNAME>"`

# Notes

# Useful websites
