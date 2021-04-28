FROM devkitpro/devkitarm:latest

RUN sudo apt-get -y update && sudo apt-get -y install python3 python3-pip 

# Install makerom
RUN curl -L https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip | zcat > makerom && chmod +x makerom && install makerom /usr/local/bin/makerom

# Install firmtool
RUN pip3 install -U git+https://github.com/TuxSH/firmtool.git

WORKDIR /src

COPY . .

