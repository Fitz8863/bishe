SECRET_KEY='asdmmnkdnlamdl;awwd'

# 数据库基本信息
HOSTNAME='127.0.0.1'
PORT=3306
USERNAME='root'
PASSWORD='heweijie'
DATABASE='bishe'
DB_URI = 'mysql+pymysql://{}:{}@{}:{}/{}?charset=utf8'.format(USERNAME,PASSWORD,HOSTNAME,PORT,DATABASE)
SQLALCHEMY_DATABASE_URI = DB_URI

# 邮箱配置
# qvqnpnqkirdldgbb
MAIL_SERVER='smtp.qq.com'
MAIL_USE_SSL=True
MAIL_PORT=465
MAIL_USERNAME='3189801930@qq.com'
MAIL_PASSWORD='efcupjqhgltfddaj'
MAIL_DEFAULT_SENDER='3189801930@qq.com'

# MQTT配置
MQTT_BROKER='127.0.0.1'
MQTT_PORT=1883
MQTT_USERNAME=''
MQTT_PASSWORD=''
MQTT_TOPIC_PREFIX='jetson/camera/command'