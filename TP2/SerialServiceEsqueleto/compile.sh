#! /usr/bin/env bash

gcc -Wall -Wextra serial_service.c SerialManager.c -pthread -o serial_service

