# EMCSE

EarthMC Query & Analysis Tool (Qt / C++)

## Main Features
- Query Player / Town / Nation
- JSON visualization
- Plugin system (runtime loading)

## Development Environment
- Visual Studio 2022  
- Qt 6.9.2 (MSVC 2022 x64)  
- Qt VS Tools  

## Project Structure
- `EMCSE/`: Host program  
- `OfflineWarningPlugin/`: Example plugin 
- `IAnalysisPlugin.h`: Plugin interface  

## Build
1. Open `EMCSE.sln`  
2. Select `Debug|x64` or `Release|x64`  
3. Compile both host program and plugin project  

## Release
Use `windeployqt` to deploy the `Release` build on Windows  

## Roadmap
[] Cross-platform adaptation: Linux, Android, macOS  
[] EMCSE server-side  
[] Town Ruins Status Forecast

---

# EMCSE

EarthMC 查询与分析工具（Qt / C++）

## 主功能
- 查询 Player / Town / Nation  
- JSON 可视化展示  
- 插件系统（运行时加载）  

## 开发环境
- Visual Studio 2022  
- Qt 6.9.2 (MSVC 2022 x64)  
- Qt VS Tools  

## 项目结构
- `EMCSE/`：宿主程序  
- `OfflineWarningPlugin/`：示例插件
- `IAnalysisPlugin.h`：插件接口  

## 构建
1. 打开 `EMCSE.sln`  
2. 选择 `Debug|x64` 或 `Release|x64`  
3. 编译宿主程序和插件项目  

## 发布
Windows 下使用 `windeployqt` 部署 `Release` 版  

## 计划
[] Linux、Android、macOS 跨平台适配  
[] EMCSE 服务端 
[] 城镇废墟全局搜索预报 
