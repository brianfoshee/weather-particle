package main

import (
	"context"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/signal"

	"google.golang.org/api/iterator"
	"google.golang.org/api/option"

	"cloud.google.com/go/pubsub"
)

const subscriptionName = "wundergroundPusher"

func main() {
	ctx := context.Background()

	client, err := pubsub.NewClient(ctx, "particle-gcp-playground", option.WithServiceAccountFile("particle-gcp-playground-3135d48f51d0.json"))
	if err != nil {
		log.Fatal("could not create pubsub client")
	}

	topic := client.Topic("weather")
	ok, err := topic.Exists(ctx)
	if err != nil {
		log.Fatal("could not check if topic exists", err)
	}
	if !ok {
		log.Fatal("topic should exist")
	}

	sub := client.Subscription(subscriptionName)
	ok, err = sub.Exists(ctx)
	if err != nil {
		log.Fatal("could not check if subscription exists")
	}
	if !ok {
		// Subscription doesn't exist.
		sub, err = client.CreateSubscription(ctx, subscriptionName, topic, 0, nil)
		if err != nil {
			log.Fatal("could not create subscription")
		}
	}

	// Construct the iterator
	it, err := sub.Pull(context.Background())
	if err != nil {
		log.Fatal("could not pull messages")
	}

	go func() {
		c := make(chan os.Signal, 1)
		signal.Notify(c, os.Interrupt)

		<-c
		log.Println("Shutting down...")
		it.Stop()
	}()

	for {
		msg, err := it.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			log.Println("error getting next message", err)
			break
		}

		// msg.ID
		// msg.Attributes["device_id"]
		// msg.Attributes["event"]
		// msg.Attributes["published_at"] // published_at=2016-11-19T02:03:50.138Z
		err = sendToWU(msg.Data)
		if err == nil {
			msg.Done(true)
		}
	}

	log.Println("Goodbye")
}

const host = "https://weatherstation.wunderground.com"
const path = "/weatherstation/updateweatherstation.php"

var client = &http.Client{}

func sendToWU(d []byte) error {
	str := string(d)
	log.Print("got message: ", string(d))

	ov, err := url.ParseQuery(str)
	if err != nil {
		return err
	}

	v := url.Values{}
	v.Set("ID", "")
	v.Set("PASSWORD", "")
	v.Set("dateutc", "now")
	v.Set("tempf", ov.Get("tf"))
	v.Set("dewptf", ov.Get("df"))
	v.Set("humidity", ov.Get("h"))
	v.Set("baromin", ov.Get("b"))
	v.Set("softwaretype", "Particle-Photon")
	v.Set("action", "updateraw")

	resp, err := client.Get(host + path + "?" + v.Encode())
	if err != nil {
		return err
	}
	resp.Body.Close()

	log.Printf("received response code %d after sending data %s", resp.StatusCode, v.Encode())

	return nil
}

/*
  client.print("ID=");
  client.print(ID);
  client.print("&PASSWORD=");
  client.print(PASSWORD);
  client.print("&dateutc=now");      //can use 'now' instead of time if sending in real time
  client.print("&tempf=");
  client.print(tempF);
  client.print("&dewptf=");
  client.print(dewptF);
  client.print("&humidity=");
  client.print(humidity);
  client.print("&baromin=");
  client.print(inches);
  //
  client.print("&winddir=");
  client.print(winddir);
  client.print("&windspeedmph=");
  client.print(windspeedmph);
  client.print("&windgustdir=");
  client.print(windgustdir);
  client.print("&windspdmph_avg2m=");
  client.print(windspdmph_avg2m);
  client.print("&winddir_avg2m=");
  client.print(winddir_avg2m);
  client.print("&windgustmph_10m=");
  client.print(windgustmph_10m);
  client.print("&windgustdir_10m=");
  client.print(windgustdir_10m);
  client.print("&rainin=");
  client.print(rainin);
  client.print("&dailyrainin=");
  client.print(dailyrainin);
  //
  client.print("&softwaretype=Particle-Photon");
  client.print("&action=updateraw");    //Standard update rate - for sending once a minute or less
*/
