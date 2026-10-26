# 公开归档验证

2026-10-26在独立的公开仓库副本中重新验证，原赛时工作区保持不变。

| 检查 | 结果/范围 |
|---|---|
| 整理复制 | 入库文件逐一核对SHA256，复制一致；公开处理后更新清单 |
| V17主程序 | 当前工程、交接工程、独立V17副本SHA256相同 |
| Keil工程源文件 | Project.uvprojx引用的相对源文件均存在 |
| 重新Rebuild | 0 errors / 0 warnings，Code=19356、RO-data=8144、RW-data=68、ZI-data=3724 |
| 历史UART二进制协议 | 打包、CRC、拆包、错误帧拒收用例通过；与V17 ASCII协议分开 |
| 公开配置处理 | 8处无线AP密码替换为CHANGE_ME_123；2个SDK示例私钥留在本地 |

完整的新构建日志见`validation/rebuild-publication.log`。此次没有连接或重新烧录实车，因此不新增运动性能或比赛达标结论。GitHub Actions将继续检查文件清单、工程路径和无需硬件的源码编译。
