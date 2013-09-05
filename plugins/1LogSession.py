import sqlite3

SQLPATH = "/home/hugsy/http_session.log"
conn, cur = (None, None)
BUILD_TABLE_SQL = """CREATE TABLE requests (id INTEGER, request BLOB, inserted TIMESTAMP DEFAULT CURRENT_TIMESTAMPDATE);"""
BUILD_TABLE_SQL+= """CREATE TABLE responses (id INTEGER, response BLOB, inserted TIMESTAMP DEFAULT CURRENT_TIMESTAMPDATE);"""
REQ_SQL = """INSERT INTO requests (id, request) VALUES (?, ?)"""
RES_SQL = """INSERT INTO responses (id, response) VALUES (?, ?)"""

def proxenet_request_hook(request_id, request):
    conn = sqlite3.connect(SQLPATH)
    if conn:
        cur = conn.Cursor()
        cur.execute(BUILD_TABLE_SQL)
        cur.execute(REQ_SQL, request_id, request)
        conn.commit()
        
    return str(r)

    
def proxenet_response_hook(response_id, response):
    if conn:
        cur.execute(RES_SQL, response_id, response)
        conn.commit()
        conn.close()
    
    return response


if __name__ == "__main__":
    # todo add test cases
    pass    
