#!/bin/bash
for i in `seq 1 3000`;
        do
		sleep 1
                ./paydaycoin-cli generate 10
        done    
