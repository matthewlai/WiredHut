#!/bin/sh

SCP=/usr/bin/scp
SSH=/usr/bin/ssh

${SCP} * homeserver:daemons/light_controller/
${SSH} homeserver chmod a+x daemons/light_controller/light_controller.py
${SSH} homeserver sudo systemctl restart light_controller
