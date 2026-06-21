simple_httpd.cは簡単なhttpサーバプログラムです。
GETメソッドのみを受け付けます。
第2回課題同様、Makefileがあります。
1. makeします
```sh
$ make
```

2. サーバを実行します。Makefileに定義された run ターゲットを指定して make を実行します。すなわち、以下の通り。
```sh
$ make run
```
これでサーバが起動します。

3. Browserで、<br>
  http://localhost:8080<br>
  と入力してみましょう。index.htmlの内容が表示されます。

4. Browserで、<br>
  http://localhost:8080/index2.html<br>
  と入力してみましょう。index2.htmlの内容が表示されます。

5. docというディレクトリを作り、その下で、page.htmlというファイルを作ってみましょう。
```sh
$ mkdir doc
$ cd doc
$ vim page.html
```

6. doc/page.htmlを開いてみましょう。<br>
  http://localhost:8080/index2.html

7. このプログラムにもいくつか問題点があります。AIにどこが問題か聞いてみましょう。

