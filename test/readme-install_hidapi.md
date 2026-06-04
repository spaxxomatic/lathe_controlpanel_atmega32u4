
pip uninstall hid

pip install hidapi Cython


git clone https://github.com/trezor/cython-hidapi.git  
cd cython-hidapi  
git submodule update --init    
python setup.py build 
python setup.py install   
pip install -e .