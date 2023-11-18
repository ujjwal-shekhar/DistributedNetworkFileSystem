#!/bin/bash

# Clean and build
make veryclean && make

# Number of SSx folders (change this as needed)
num_ss_folders=3

# Iterate over SSx folders
for ((i=1; i<=$num_ss_folders; i++)); do
    ss_folder="SS/SS$i"

    # Create the SSx folder if it doesn't exist
    mkdir -p "$ss_folder"

    # Copy the executable to the SSx folder
    cp server "$ss_folder/"
done

echo "Build and copy process completed."
