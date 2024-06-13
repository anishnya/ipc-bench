#!/bin/bash
import subprocess
import re
from grapher import graphMultipleLines
import os

FIFO = "fifo"
SOCKET = "socket"
TCP = "tcp"
BASE_PATH = "/home/anishnya/ipc-bench/benchmarkOutput"

IPCS = [
    FIFO,
]

PATH_MAP = {
    FIFO: f"{BASE_PATH}/fifo",
}

def getSizeInfoFilename(filename):
    pattern = r"(\d+)_(\d+)"
    match = re.search(pattern, filename)

    if match:
        size, _ = match.groups()
        return size
    else:
        exit(1)

def parseOutput(output):
    nums = []
    for q in [r"Message rate:\s+([0-9]+)", r"Latency:\s*([+-]?\d+\.\d+)"]:
        match = re.search(q, output)

        if match:
            number = float(match.group(1))
            nums.append(number)
        else:
            print("Number not found in the string.")
    
    return nums

def readFile(filename):
    with open(filename, "r") as file:
        return str(file.read())

def getStats(filename, statMap):
    size = getSizeInfoFilename(filename)
    throughPut, latency = parseOutput(readFile(filename))

    if size not in statMap:
        statMap.update({size: [[], []]}) 

    statMap[size][0].append(throughPut)
    statMap[size][1].append(latency)

def getInfo(ipc):
    directory = PATH_MAP[ipc]
    filenames = os.listdir(directory)
    statMap = {}

    for filename in filenames:
        fullPath = os.path.join(directory, filename)
    
        if os.path.isfile(fullPath):
            getStats(fullPath, statMap)
    
    return statMap

def runBenchmarks():
   for ipc in IPCS:
        resultMap = getInfo(ipc)
        print(resultMap)
        # graph when done
        graphMultipleLines("Throughput (msg/sec)", "Latency (us)", ipc, resultMap)
        print(resultMap)
    
if __name__ == "__main__":
   runBenchmarks()