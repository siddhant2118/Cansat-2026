import os
import subprocess
import sys
import shutil

def clean():
    for d in ["build", "dist"]:
        if os.path.exists(d):
            shutil.rmtree(d)
    if os.path.exists("CansatGCS.spec"):
        os.remove("CansatGCS.spec")

def build():
    # Define platform specific separators
    sep = os.pathsep # ; on windows, : on unix
    
    # Data arguments (add config.yaml)
    # Format: src:dest
    add_data = f"config.yaml{os.pathsep}."
    
    cmd = [
        "pyinstaller",
        "--name=CansatGCS",
        "--onefile",
        "--windowed", # No terminal on Mac/Windows
        f"--add-data={add_data}",
        # "--icon=resources/icon.ico", # If we have one
        "main.py"
    ]
    
    print(f"Building: {' '.join(cmd)}")
    subprocess.check_call(cmd)
    
    print("\n[SUCCESS] Build complete. Executable is in 'dist/' folder.")

if __name__ == "__main__":
    clean()
    try:
        build()
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Build failed: {e}")
        sys.exit(1)
