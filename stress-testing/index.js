const Pop3Command = require('node-pop3');
const NetcatClient = require('netcat/client');

const pop3 = new Pop3Command(
    {
        user: "hola",
        password: "chau",
        host: "localhost",
        port: 1110
    }
);

async function start() {
    const nc = new NetcatClient();
    const client = nc.addr('::1').port(2110).connect().send('a root root\r\n').on('data', (buffer) => {
    });
    let i = { count: 0 };
    for (let i = 0; i < 900; i += 1) {
        client.send(`u+ ${i} ${i}\r\n`, (ca) => {
            console.log(ca, i);
            i.count += 1;
            if (i.count === 900) {
                client.close();
            }
        })
    }
    client.send('q\r\n');
    const str = await pop3.LIST();
    console.log(str);
    await pop3.QUIT();
}

start();