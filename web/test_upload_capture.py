#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟远程摄像头上传抓拍图片
用法: python test_upload_capture.py <图片路径> <camera_id> <location> <violation_type>
"""

import os
import sys
import requests


def upload_capture(image_path, camera_id, location, violation_type, server_url='http://127.0.0.1:5000'):
    """
    上传抓拍图片
    
    参数必须是完整的:
        image_path: 图片文件路径
        camera_id: 摄像头编号
        location: 抓拍地点
        violation_type: 违规类型
    """
    if not os.path.exists(image_path):
        print(f"[失败] 文件不存在: {image_path}")
        return False
    
    allowed_extensions = {'png', 'jpg', 'jpeg', 'gif'}
    file_ext = image_path.rsplit('.', 1)[-1].lower()
    if file_ext not in allowed_extensions:
        print(f"[失败] 不支持的文件格式: {file_ext}")
        return False
    
    url = f"{server_url}/capture/upload"
    
    try:
        with open(image_path, 'rb') as f:
            files = {'file': (os.path.basename(image_path), f)}
            data = {
                'camera_id': camera_id,
                'location': location,
                'violation_type': violation_type
            }
            
            print(f"[信息] 上传中: {image_path}")
            response = requests.post(url, files=files, data=data, timeout=10)
            result = response.json()
            
            if result.get('code') == 200:
                print(f"[成功] {result.get('message')}")
                return True
            else:
                print(f"[失败] {result.get('message')}")
                return False
                
    except requests.exceptions.ConnectionError:
        print("[失败] 无法连接到服务器")
        return False
    except Exception as e:
        print(f"[失败] {str(e)}")
        return False


def main():
    if len(sys.argv) < 5:
        print("用法: python test_upload_capture.py <图片路径> <camera_id> <location> <violation_type>")
        print("示例: python test_upload_capture.py alert.jpg 001 生产车间A区 未佩戴安全帽")
        return
    
    image_path = sys.argv[1]
    camera_id = sys.argv[2]
    location = sys.argv[3]
    violation_type = sys.argv[4]
    
    upload_capture(image_path, camera_id, location, violation_type)


if __name__ == '__main__':
    main()
