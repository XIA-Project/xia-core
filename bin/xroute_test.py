import subprocess
subprocess.__file__

# The input string to be passed as a command line argument to the C++ program
input_string = "Type4,12346,eth0,192.168.1.1"
add = '-a'
# Call the C++ program as a subprocess and pass the input string as a command line argument
process = subprocess.Popen(["./forwarding_table", add,input_string], stdout=subprocess.PIPE)

# Read the output of the subprocess and print it to the console
output, errors = process.communicate()
if errors:
    print("Errors occurred:", errors.decode())
print(output.decode())
