#!/usr/bin/env python3

import os
from pathlib import Path
import zipfile
import shutil
import subprocess
import time

# path to all submissions folder "/home/aaranya/projs/cn42/endlab/1155_CS F303-Lab Compre-L1-10785/"
submissions_path = "/home/aaranya/projs/cn42/endlab/1155_CS F303-Lab Compre-L1-10785/"

# path to router.c folder
router_folder = "user/src/"

# path to makefile
makefile_folder = "user/"

# Results folder
results_folder = "results/"

# Map for storing name and score
name_score = {}

counter = 0
score = 0

# Evaluate source files at path
def evaluate(path, name):
    global counter, score, name_score

    # Remove old router.c, packet.c, application.c
    # if os.path.exists(os.path.join(router_folder, "router.c")):
    #     os.remove(os.path.join(router_folder, "router.c"))
    if os.path.exists(os.path.join(router_folder, "packet.c")):
        os.remove(os.path.join(router_folder, "packet.c"))
    # if os.path.exists(os.path.join(router_folder, "application.c")):
    #     os.remove(os.path.join(router_folder, "application.c"))

    try:
    # Copy new router.c, packet.c, application.c from path/ to router_folder
        # shutil.copy(os.path.join(path, "router.c"), router_folder)
        shutil.copy(os.path.join(path, "packet.c"), router_folder)
        # shutil.copy(os.path.join(path, "application.c"), router_folder)
    except:
        print("Error copying files to router folder")

    
    res = ""
    # Run make with route option
        # res = subprocess.(["make", "route"], cwd=makefile_folder)
        # Store output of make in res
    res = subprocess.run(["make", "route"], cwd=makefile_folder, capture_output=True, text=True, errors="ignore")

    # Find score
    # [*] Test 1 [insufficient size]: 2 / 2 points
    # ERROR TEST 2:
    # [*] Test 2 [insufficient size]: 2 / 2 points

    score_temp = 0
    # Find score
    for line in res.stderr.split("\n"):
        if line.startswith("[*] Test"):
            # Split by space and find score which is a number just before slash
            score_temp += int(line.split(" ")[-4])
            
    print("Score: " + str(int((score_temp+1)/2)))
    score += score_temp
    counter += 1

    # Save output to file name in results folder
    with open(os.path.join(results_folder, name), "w") as f:
        # Writing ouput for
        print("Writing output for " + name + " to file" + os.path.join(results_folder, name))
        # Write stderr to file
        f.write(res.stderr)
        # Write stdout to file
        f.write(res.stdout)
        # Write name to file
        f.write("Name: " + name + "\n")
        # Write score to file
        f.write("Score: " + str(score_temp) + "\n")
        # Scaled down score
        f.write("Scaled down score: " + str(int((score_temp+1)/2)) + "\n")
    
    # Add to map
    name_score[name] = int((score_temp+1)/2)
        




# Fetch source file location and evaluate
def fetch_evaluate(path):

    # Check if path exists
    if not os.path.exists(path):
        print("Path does not exist")
        return
    
    # name upto first . in current directory name
    name = os.path.basename(path).split(".")[0]
    print("Name: " + name)

    temp = ""
    # Unzip any zips in folder
    for file in os.listdir(path):
        if file.endswith(".zip"):
            # Call folder temp
            temp = os.path.join(path, "temp")
            # Unzip file
            with zipfile.ZipFile(os.path.join(path, file), 'r') as zip_ref:
                zip_ref.extractall(temp)

    
    # Find router.c
    for path in Path(path).rglob('packet.c'):
        print("Found at:" + path.__str__())
        # Evaluate with path of router.c's folder
        evaluate(path.parent, name)

    
    # Delete temp folder, if it exists
    if os.path.exists(temp):
        print("Deleting temp folder")
        shutil.rmtree(temp)
    else:
        print("No temp folder found")
    
    return

# main
if __name__ == "__main__":
    # Timer
    start = time.time()

    # Is valid directory
    if os.path.isdir(submissions_path):
        # Fetch router.c in every subfolder in path
        for folder in os.listdir(submissions_path):
            if os.path.isdir(os.path.join(submissions_path, folder)):
                print("Evaluating " + os.path.join(submissions_path, folder))
                fetch_evaluate(os.path.join(submissions_path, folder))
        # Print score, counter and average
        score = (score+1)/2
        score = int(score)
        print("Score: " + str(score))
        print("Counter: " + str(counter))
        print("Average: " + str(score/counter))

    else:
        print("Invalid directory path")

    # Write name_score to file as csv
    with open(os.path.join(results_folder, "name_score.csv"), "w") as f:
        for name in name_score:
            f.write(name + "," + str(name_score[name]) + "\n")


    
    # Timer
    end = time.time()
    print("Time taken: " + str(end - start) + " seconds")

    
            
