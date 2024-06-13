import matplotlib.pyplot as plt
import numpy as np


def graphMultipleLines(xlabel, ylabel, filename, data):
    for k,v in data.items():
        plt.plot(v[0], v[1], label=k)

    # Add labels and title
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)

    # Add legend
    plt.legend()

    plt.savefig("graphs/" + filename + ".png", dpi=300, bbox_inches='tight')