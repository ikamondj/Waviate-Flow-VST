#RUN THIS WITH: `python3 setupValidator.py build_ext --inplace`
import sys
from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "json_validator",
        sources=["../Source/Validator.cpp"],
        include_dirs=[
            pybind11.get_include(),
            "../Source",  # Add other include dirs as needed
        ],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="json_validator",
    version="0.1",
    ext_modules=ext_modules,
)