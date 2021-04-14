cat << EOF | debconf-set-selections 
phpmyadmin phpmyadmin/reconfigure-webserver string ''
phpmyadmin phpmyadmin/dbconfig-install string 'true' 
phpmyadmin phpmyadmin/mysql/app-pass string 'password'
phpmyadmin phpmyadmin/mysql/admin-pass string 'password'
EOF

# https://dev.mysql.com/doc/refman/5.7/en/resetting-permissions.html
# # msql -u root
# > FLUSH PRIVILEGES;
# > ALTER USER 'root'@'localhost' IDENTIFIED BY 'password';
# > quit;
# mysql --user=root --execute="FLUSH PRIVILEGES; ALTER USER 'root'@'localhost' IDENTIFIED BY 'password'; quit;"
# Open a browser and go to http://localhost
# phpMyAdmin login is root, password
