#!/bin/bash
# IntelliSense 诊断脚本

echo "=== IntelliSense 诊断 ==="
echo ""

echo "1. 检查必要文件:"
if [ -f "compile_commands.json" ]; then
    echo "  ✓ compile_commands.json 存在"
    if [ -L "compile_commands.json" ]; then
        echo "  ✓ 是符号链接"
        echo "  → 指向: $(readlink compile_commands.json)"
    else
        echo "  ⚠ 不是符号链接"
    fi
else
    echo "  ✗ compile_commands.json 不存在"
fi

if [ -f ".vscode/c_cpp_properties.json" ]; then
    echo "  ✓ c_cpp_properties.json 存在"
else
    echo "  ✗ c_cpp_properties.json 不存在"
fi

echo ""
echo "2. 检查编译器:"
if command -v g++ &> /dev/null; then
    echo "  ✓ g++ 已安装: $(g++ --version | head -1)"
else
    echo "  ✗ g++ 未安装"
fi

echo ""
echo "3. 检查 Qt 路径:"
if [ -d "/home/lsy/Qt/6.9.2/gcc_64/include" ]; then
    echo "  ✓ Qt 头文件路径存在"
    qt_files=$(find /home/lsy/Qt/6.9.2/gcc_64/include/QtCore -name "*.h" 2>/dev/null | wc -l)
    echo "  → QtCore 头文件数量: $qt_files"
else
    echo "  ✗ Qt 头文件路径不存在"
fi

echo ""
echo "4. 检查 compile_commands.json 内容:"
if [ -f "compile_commands.json" ]; then
    count=$(grep -c "\"file\"" compile_commands.json 2>/dev/null || echo "0")
    echo "  → 包含 $count 个文件的编译命令"
    
    if grep -q "main.cpp" compile_commands.json 2>/dev/null; then
        echo "  ✓ 包含 main.cpp 的编译命令"
    else
        echo "  ✗ 未找到 main.cpp 的编译命令"
    fi
fi

echo ""
echo "5. 检查 IntelliSense 数据库:"
if [ -f ".vscode/browse.vc.db" ]; then
    size=$(du -h .vscode/browse.vc.db 2>/dev/null | cut -f1)
    echo "  ✓ 数据库文件存在，大小: $size"
else
    echo "  ⚠ 数据库文件不存在（首次索引后会创建）"
fi

echo ""
echo "=== 诊断完成 ==="
echo ""
echo "如果所有检查都通过，请在 Cursor 中:"
echo "1. 按 Ctrl+Shift+P"
echo "2. 输入: C/C++: Reset IntelliSense Database"
echo "3. 等待重新索引完成"




