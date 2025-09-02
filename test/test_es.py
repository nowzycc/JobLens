import os
import unittest
import requests
import json

# -------------------- 配置 --------------------
ES_HOST = os.getenv("ES_HOST", "http://192.168.51.85:9200")
ES_AUTH = (
    os.getenv("ES_USER", ""),
    os.getenv("ES_PASS", "")
)
ES_VERIFY_CERTS = os.getenv("ES_VERIFY_CERTS", "true").lower() != "false"

# 统一 requests session，保持连接池、认证、SSL 配置
session = requests.Session()
if ES_AUTH[0]:
    session.auth = ES_AUTH
session.verify = ES_VERIFY_CERTS


# -------------------- 工具函数 --------------------
def url(path: str) -> str:
    """拼接完整 URL"""
    return f"{ES_HOST.rstrip('/')}/{path.lstrip('/')}"


def assert_resp(resp: requests.Response, expected_status=(200, 201)):
    """断言状态码并返回 JSON"""
    assert resp.status_code in expected_status, \
        f"status={resp.status_code}, body={resp.text}"
    return resp.json()


# -------------------- 测试用例 --------------------
class TestElasticsearchAPI(unittest.TestCase):
    TEST_INDEX = "test_py_demo"

    @classmethod
    def setUpClass(cls):
        """类级初始化：删除遗留索引再创建"""
        session.delete(url(f"/{cls.TEST_INDEX}"))  # 先清理
        mapping = {
            "mappings": {
                "properties": {
                    "title": {"type": "text"},
                    "date":  {"type": "date"}
                }
            }
        }
        resp = session.put(url(f"/{cls.TEST_INDEX}"), json=mapping)
        assert_resp(resp)

    @classmethod
    def tearDownClass(cls):
        """类级清理：删除测试索引"""
        session.delete(url(f"/{cls.TEST_INDEX}"))

    def test_health(self):
        """1. 集群健康检查"""
        data = assert_resp(session.get(url("/_cluster/health")))
        self.assertEqual(data["status"] in {"green", "yellow"}, True)

    def test_index_exists(self):
        """2. 索引存在性"""
        resp = session.head(url(f"/{self.TEST_INDEX}"))
        self.assertEqual(resp.status_code, 200)

    def test_doc_crud(self):
        """3. 单条文档 CRUD"""
        doc = {"title": "hello ES", "date": "2025-08-27"}
        # 创建
        res = assert_resp(session.post(
            url(f"/{self.TEST_INDEX}/_doc"), json=doc))
        _id = res["_id"]
        # 读取
        read = assert_resp(session.get(
            url(f"/{self.TEST_INDEX}/_doc/{_id}")))
        self.assertEqual(read["_source"]["title"], "hello ES")
        # 更新
        update_body = {"doc": {"title": "updated"}}
        assert_resp(session.post(
            url(f"/{self.TEST_INDEX}/_update/{_id}"), json=update_body))
        # 删除
        assert_resp(session.delete(
            url(f"/{self.TEST_INDEX}/_doc/{_id}")), expected_status=(200, 404))

    def test_bulk_and_search(self):
        """4. 批量写入 + 查询验证"""
        bulk_body = ""
        for i in range(100):
            bulk_body += json.dumps({"index": {"_index": self.TEST_INDEX}}) + "\n"
            bulk_body += json.dumps({"title": f"doc-{i:03}", "date": "2025-08-27"}) + "\n"
        resp = session.post(url("/_bulk"), data=bulk_body,
                            headers={"Content-Type": "application/x-ndjson"})
        assert_resp(resp)
        # 强制刷新，保证搜索可见
        session.post(url(f"/{self.TEST_INDEX}/_refresh"))

        # DSL 查询
        query = {
            "query": {"match": {"title": "doc-01"}},
            "_source": ["title"]
        }
        data = assert_resp(session.post(
            url(f"/{self.TEST_INDEX}/_search"), json=query))
        self.assertEqual(data["hits"]["total"]["value"], 1)
        self.assertEqual(data["hits"]["hits"][0]["_source"]["title"], "doc-010")


# -------------------- 入口 --------------------
if __name__ == "__main__":
    unittest.main(verbosity=2)