# Troubleshooting freeipa
## freeipa certificates expired
* Confirm by looking at the certs:
`# getcert list`
* Rollback the clock by disabling chronyd
`# systemctl stop chronyd`
* Set the date back to a time when the certificates were still valid
* For each Request ID, run ipa-getcert resubmit
```
# ipa-getcert resubmit -i 20240415020448
# ipa-getcert resubmit -i 20240415020449
# ipa-getcert resubmit -i 20240415020450
# ipa-getcert resubmit -i 20240415020451
# ipa-getcert resubmit -i 20240415020455
# ipa-getcert resubmit -i 20240415020530
# ipa-getcert resubmit -i 20240415020536
```
* Set the date back forward by reenabling chronyd
`# systemctl start chronyd`
* Restart ipa:
`# ipactl restart`
* Check status:
`# ipactl status`
