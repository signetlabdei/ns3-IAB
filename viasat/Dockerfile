FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get install -y \
        git \
        gcc \
        g++ \
        vim \
        python3 \
        python3-dev \
	python3-pip \
        python3-setuptools \
        autoconf \
        bzr \
        unrar \
        gdb \
        valgrind \
        uncrustify \
        flex \
        bison \
        libfl-dev \
        tcpdump \
        gsl-bin \
	libgslcblas0 \
        libgsl-dev \
        sqlite \
        sqlite3 \
        libsqlite3-dev \
        libxml2 \
        libxml2-dev \
        cmake \
        libc6-dev \
        libc6-dev-i386 \
        libclang-dev \
        llvm-dev \
        automake \
        libgtk2.0-0 \
        libgtk2.0-dev \
        vtun \
        lxc \
        libboost-filesystem-dev \
	texlive-latex-base texlive-latex-extra jq unzip texlive-fonts-recommended dvipng cm-super \
        python3-sphinx dia  texlive-extra-utils texlive-font-utils latexmk libcanberra-gtk3-module doxygen graphviz

RUN python3 -m pip install sem numpy matplotlib==3.5.2 tikzplotlib scienceplots latex
WORKDIR /ns3
CMD /bin/bash
