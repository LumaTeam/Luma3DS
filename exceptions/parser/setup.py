from setuptools import setup, find_packages

setup(
    name='exception_dump_parser',
    version='1.2',
    url='https://github.com/AuroraWright/Luma3DS',
    author='TuxSH',
    license='GPLv3',
    description='Parses Luma3DS exception dumps',
    install_requires=[''],
    packages=find_packages(),
    entry_points={'console_scripts': ['exception_dump_parser=exception_dump_parser.exception_dump_parser:main']},
)
