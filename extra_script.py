Import("env")

# Install a specific package
env.Execute("$PYTHONEXE -m pip install windows-curses")
