# Application overview
* Denial of service attacks on Nginx using hping3
* Username guessing attacks (via RCPT TO) on Postfix using ismtp
* Email spoofing attacks on Postfix using SEToolkit
* Brute-force dictionary attacks on Dovecot using hydra
* Firefox web client
* Thunderbird email client

# Prerequisites:
* SSH access to host
* Install sshpass: `$ sudo apt-get install sshpass`
* Create ssh keys for your user: `$ ssh-keygen -t ed25519 -f id_ed25519 -C <USERNAME>`
* Copy the id_ed25519.pub contents into ~/.ssh/authorized_keys
* Copy the key to the container: `$ ssh-copy-id -i ~/.ssh/id_ed25519.pub -p 2222 <USERNAME>@localhost`
* Test the key: `$ ssh -i ~/.ssh/id_ed25519 <USERNAME>@localhost -p 2222`

# Runtime environment setup
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/main/applications/attack/attack-KaliX86-64.sh
    $ chmod +x attack-KaliX86-64.sh
    $ ./attack-KaliX86-64.sh
    ```
1. Create a Kali linux container using the instructions from [here](https://github.com/jhu-information-security-institute/netsec-kalilinux-docker)
1. Create a venv: `$ python3 -m venv ansible`
1. Activate your new venv: `$ source ansible/bin/activate`
1. Install ansible into the venv: `(ansible)$ pip install ansible`
1. Create your `inventory` and `inventory/group_vars` folders
1. Create your inventory files in the `inventory` folder.  E.g., create `01-attack.yml` with the following contents
    ```
    kali2024:
      hosts:
        localhost:
    ```
1. Create your `all.yml` variables file in the `inventory/group_vars` folder.  Example contents are:
    ```
    ansible_connection: ssh
    ansible_port: 2222
    ```
1. Verify the inventory: `(ansible)$ ansible-inventory -i inventory --list`
1. Ping the hosts in your inventory: `(ansible)$ ansible <HOSTNAME> -m ping -i inventory`
1. Run the playbook: `(ansible)$ ansible-playbook setup.yml -v -i inventory --extra-vars "USERNAME=<USERNAME>"`

# Notes

# Useful websites
