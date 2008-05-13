#!/usr/bin/env ruby
#
# Copyright (c) 2008 The Kaphan Foundation
#
# Possession of a copy of this file grants no permission or license
# to use, modify, or create derivate works.
# Please contact info@peerworks.org for further information.
#

require File.dirname(__FILE__) + "/spec_helper.rb"

describe "Tag Index" do
  before(:each) do 
    system("rm /tmp/perf.log")   
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should fetch GET tag index on load" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        req.request_method.should == 'GET'
      end
    end
    
    @http.should have_received_requests
  end
  
  it "should handle 404 of tag index load" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.status = 404
      end
    end
    
    @http.should have_received_requests
  end
  
  it "should handle junk in the response" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = "blah blah blah"
      end
    end
    
    @http.should have_received_requests
  end
  
  it "should handle real content" do
    @http.should_receive do 
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
    end
    
    @http.should have_received_requests
  end
end

describe "after item addition" do
  before(:each) do
    start_tokenizer
    system("rm /tmp/perf.log")   
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom', :sleep => false)
    @http = TestHttpServer.new(:port => 8888)
  end

  after(:each) do
    stop_tokenizer
    stop_classifier
  end

  it "should fetch again after adding an item" do
    requests = 0
    @http.should_receive do
      request("/tags.atom", 2) do |req, res|
        requests += 1
        if requests == 1
          res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
        else
          req['IF-MODIFIED-SINCE'].should == "Mon, 12 May 2008 02:42:14 GMT"
          res.status = 304
        end
      end
    end

    sleep(0.1)
    create_entry
    @http.should have_received_requests
  end    
end