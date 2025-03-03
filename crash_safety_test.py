import os
from subprocess import call
from multiprocessing import Process
import time
import re
from subprocess import Popen, PIPE

max_times = 30
test = "lab4test_c"
output_file = "output.txt"
ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

def main():
    garbage = open("garbage.txt", 'w')
    call(["make","clean"], stdout = garbage, stderr = garbage)

    if os.path.exists(output_file):
        os.remove(output_file)
    print("clean finished.")

    call(["make"], stdout = garbage, stderr = garbage)
    print("make finished.")

    w = open(output_file, 'w')

    print("Running " + test + " " + str(max_times) + " times. test output=" + output_file)
    process = Popen([r'make', 'qemu'], stdin=PIPE, stdout=w)
    time.sleep(5)
    i = 1
    while process.poll() == None:
        test_str = "lab4test_c " + str(i) + "\n"
        process.stdin.write(test_str.encode())
        process.stdin.flush()
        time.sleep(1)
        print("finished i=" + str(i))
        i += 1
        if i > max_times:
            break
    process.terminate()
    print("killing qemu")
    call(["pkill","qemu"], stdout = garbage, stderr = garbage)
    garbage.close()
    w.close()
    os.remove("garbage.txt")

    r = open(output_file, 'r')
    buf = r.read()
    result = ansi_escape.sub('', buf)
    if "Not consistent" in result:
        print("file system is not crash-safe")
    elif "passed lab4 part c test!" in result:
        print("file system is crash-safe")
    else:
        print(f"Test failed: system did not successfully pass lab 4 part c test within {max_times} bwrites!")

    r.close()

if __name__ == "__main__":
    main()
