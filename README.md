# 6340 Project
Based on [SVF-example](https://github.com/SVF-tools/SVF-example)


## Prerequisite
```sh
apt install npm
```

```sh
npm i --silent svf-lib --prefix ${HOME}
```

```sh
./env.sh
```
Copy the enrionment variables to your rc files (e.g. `.bashrc` or `.zshrc`)

## Build
```sh
mkdir BUILD
cd BUILD
cmake ..
make
```
