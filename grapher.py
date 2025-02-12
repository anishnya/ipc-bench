import matplotlib.pyplot as plt
import numpy as np

def plotExpo(xlabel, ylabel, filename, data, label):
    x_data = np.array(data[0])
    y_data = np.array(data[1])
    
    # Transform data for linear regression
    log_y_data = np.log(y_data)

    # Perform linear regression on transformed data
    m, b = np.polyfit(x_data, log_y_data, 1)

    # Recover exponential fit parameters
    base_b = np.exp(m)
    initial_a = np.exp(b)

    # Generate x-values and calculate corresponding y-values for the exponential curve
    x_line = np.linspace(min(x_data), max(x_data), 100)
    y_line = initial_a * base_b**x_line

    # Create the plot
    plt.scatter(x_data, y_data)
    plt.plot(x_line, y_line, color='red', label=label)  # Label the line

    # Customize the plot (optional)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.savefig("graphs/" + filename + ".png", dpi=300, bbox_inches='tight')
    plt.clf()

def graphLine(xlabel, ylabel, filename, data):
    for k,v in data.items():
        plotExpo(xlabel, ylabel, filename, v, k)

def graphMultipleLines(xlabel, ylabel, filename, data):
    markers = ['o', 's', 'p', 'h', 'd', 'x', '^']
    linestyles = ['-', '--', '-.', ':']
    
    plt.figure(figsize=(2.6, 2))
    plt.ticklabel_format(style='sci', axis="x", scilimits=(0,0))
    
    for index, (k, v) in enumerate(data.items()):
        plt.plot(v[0], v[1], marker=markers[index], linestyle=linestyles[index], label=k)

    # Add labels and title
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)

    plt.ylim(top=125)

    # Add legend
    plt.legend(prop={'size': 8})
    plt.savefig("graphs/" + filename + ".pdf", bbox_inches='tight')
    plt.clf()