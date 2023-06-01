#!/bin/bash

# Get syfco
git clone https://github.com/reactive-systems/syfco.git

# Get stack
wget https://get.haskellstack.org/stable/linux-x86_64.tar.gz -O stack_x86.tar.gz
tar -zxvf stack_x86.tar.gz

stackfolder=$(find ./ -type d -name 'stack*' -printf '%f\n')
sudo cp "${stackfolder}/stack" /usr/local/bin/

cd syfco

scl enable devtoolset-7 'make -j 4'

echo "Compiled from sources available at " > ../README.txt
git remote -v | head -n 1 | awk -F ' ' '{print $2}' >> ../README.txt
echo "For commit " >> ../README.txt
git log | head -n 1 >> ../README.txt
