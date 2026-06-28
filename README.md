# Math Visualization Platform

> ⚠️ **免责声明**：本项目大部分由 deepseekV4-flash AI 生成，仅作娱乐和学习用途。
> 代码质量、正确性、安全性未经充分验证，请勿用于任何严肃或生产环境。
> 如有不足之处，欢迎友善讨论，请勿指责，谢谢！🙏

DSL-based math visualization tool with GTK+3 GUI.

## 构建 (Linux)

```bash
make
./mathviz
```

## 交叉编译 (Windows)

需要 [MXE](https://mxe.cc/):

```bash
git clone https://github.com/mxe/mxe.git
cd mxe
make gtk3          
cd /path/to/mathviz
make -f Makefile.cross MXE_DIR=/path/to/mxe
```

输出: `mathviz.exe`（静态链接，单文件，约 20MB，没有编译不知道）

## DSL 示例

```
TITLE: Curtain Model
RANGE: -2 12 -2 8
POINT: A START: 1 1 COLOR: #333
POINT: B START: 9 1 COLOR: #333
POINT: C START: 5 6 COLOR: #2ecc71
POINT: P START: 1 1 MOVE: 9 1 SPEED: 2 COLOR: #e74c3c
LINE: A B LABEL: AB COLOR: #999
LINE: A C LABEL: AC COLOR: #3498db
LINE: B C LABEL: BC COLOR: #3498db
LINE: P C LABEL: PC COLOR: #e74c3c
AREA: triangle A P C LABEL: S_APC COLOR: rgba(231,76,60,0.25)
AREA: triangle B P C LABEL: S_BPC COLOR: rgba(52,152,219,0.25)
OUTPUT: length A P LABEL: AP COLOR: #e74c3c
OUTPUT: length P B LABEL: PB COLOR: #3498db
OUTPUT: area A P C LABEL: S_APC COLOR: #e74c3c
OUTPUT: area B P C LABEL: S_BPC COLOR: #3498db
OUTPUT: area A B C LABEL: S_ABC COLOR: #9b59b6
CHECK: P.x > 4 MESSAGE: P 越过中点!
KEYPOINT: 4.0 中点
```
