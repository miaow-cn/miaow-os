# Load user's default bashrc first
if [ -f ~/.bashrc ]; then
    . ~/.bashrc
fi

# Activate the virtual environment
if [ -f "${VIRTUAL_ENV}/bin/activate" ]; then
    . "${VIRTUAL_ENV}/bin/activate"
fi