import requests
from bs4 import BeautifulSoup
import sys


def query_word(word):
    try:
        # 使用有道词典的网页版进行查询
        url = f"https://dict.youdao.com/w/{word}"
        # Ubuntu系统下的浏览器头信息，模拟Firefox在Ubuntu上的请求
        headers = {
            "User-Agent": "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0",
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.5",
            "Accept-Encoding": "gzip, deflate, br",
            "Connection": "keep-alive",
            "Upgrade-Insecure-Requests": "1",
            "Sec-Fetch-Dest": "document",
            "Sec-Fetch-Mode": "navigate",
            "Sec-Fetch-Site": "none",
            "Sec-Fetch-User": "?1"
        }

        response = requests.get(url, headers=headers)
        response.raise_for_status()  # 检查请求是否成功

        soup = BeautifulSoup(response.text, 'html.parser')

        # 提取基本释义
        explanations = []
        primary_meaning = soup.find('div', class_='trans-container')
        if primary_meaning:
            for li in primary_meaning.find_all('li'):
                explanations.append(li.text.strip())

        # 提取发音
        pronunciation = ""
        phonetic = soup.find('div', class_='phonetic')
        if phonetic:
            pronunciation = phonetic.text.strip()

        # 构建结果
        result = f"单词: {word}\n"
        if pronunciation:
            result += f"发音: {pronunciation}\n"
        if explanations:
            result += "释义:\n"
            for exp in explanations:
                result += f"- {exp}\n"
        else:
            result += "未找到该单词的释义\n"

        return result

    except Exception as e:
        return f"查询出错: {str(e)}"


if __name__ == "__main__":
    # 可以直接指定单词测试
    # word = "dog"
    # 也可以通过命令行参数传入单词
    if len(sys.argv) > 1:
        word = sys.argv[1]
        print(query_word(word))
    else:
        print("请提供要查询的单词作为命令行参数，例如: python word_query.py dog")