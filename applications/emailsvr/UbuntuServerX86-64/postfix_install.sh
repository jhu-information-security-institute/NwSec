cat << EOF | debconf-set-selections 
postfix postfix/main_mailer_type string 'Internet Site' 
postfix postfix/mailname string 'mymail.netsec.isi.jhu.edu'
EOF